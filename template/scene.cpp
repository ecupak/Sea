#include "precomp.h"
#include "scene.h"


Scene::Scene(Camera& camera)
	: _player{ &_audio_manager, camera, _WATERLINE }
{
	// Set RNG.
	//InitSeed(static_cast<uint>(time(nullptr)));
	InitSeed(1712579430);

	// Lights.
	_light_list.addDirectionalLight({ 1.0f }, 1.0f, { 1.0f, -0.5f, -1.0f });
}


void Scene::generateScene(const Stage stage)
{
	_stage = stage;

	static uint non_metal_id{ _material_list.AddNonMetal() };
	static uint emissive_id{ _material_list.AddEmissive(10.0f) };
	static uint glass_id{ _material_list.AddGlass(1.3f, 0.0f) };

	switch (stage)
	{
		case Stage::INTRO:
			generateIntro(non_metal_id, emissive_id, glass_id);
			break;
		case Stage::WORLD:
			generateWorld(non_metal_id, emissive_id, glass_id);
			break;
		default:
			break;
	}
}


void Scene::generateIntro(uint, uint, uint)
{
	
}


void Scene::generateWorld(uint non_metal_id, uint emissive_id, uint glass_id)
{
	// Triangle sea.	
	{
		uint water_id{ _material_list.AddWater(1.4f, 1.2f) };

		uint color{ 0x0000FF };

		for (int i = 0; i < 1; ++i)
		{
			float3 A{ 0.0f,					_WATERLINE, _c_upper_bounds.z };
			float3 B{ 0.0f,					_WATERLINE, 0.0f };
			float3 C{ _c_upper_bounds.x,	_WATERLINE, _c_upper_bounds.z };

			_triangles.emplace_back(A, B, C, water_id, color);

			float3 D{ _c_upper_bounds.x,	_WATERLINE, 0.0f };
			float3 E{ _c_upper_bounds.x,	_WATERLINE, _c_upper_bounds.z };
			float3 F{ 0.0f,					_WATERLINE, 0.0f };

			_triangles.emplace_back(D, E, F, water_id, color);
		}

		// Prepare BVH.
		_triangle_bvh.build();

		_bvh_list.push_back(&_triangle_bvh);
	}
		
	// Islands.
	std::vector<BVH*> _piers;
	{
		uint mirror_id{ _material_list.AddMetal() };
		
		// Create islands.
		for (int i = 0; i < _ISLAND_COUNT; ++i)
		{
			Island::Data data{ _island_data[i] };

			// Initialize island.
			switch (data._stone_type)
			{
				case Stone::Type::CHARGE:
				{
					_islands[i].initialize(i, data, non_metal_id, emissive_id);
					break;
				}
				case Stone::Type::UPGRADE:
				{
					_islands[i].initialize(i, data, non_metal_id, mirror_id);
					break;
				}
				case Stone::Type::EMPTY:
				{
					data._island_color = _island_data[data._island_color]._stone_color;
					_islands[i].initialize(i, data, non_metal_id, emissive_id);
					break;
				}
				default:
					break;
			}
			
			// Build and store BVHs.
			_bvh_list.push_back(&(_islands[i]._bvh));
			_bvh_list.push_back(&(_islands[i]._pier._bvh));
			_bvh_list.push_back(&(_islands[i]._stone._bvh));

			_piers.push_back(&(_islands[i]._pier._bvh));
		}

		// Link observers.
		for (int i = 0; i < _ISLAND_COUNT; ++i)
		{
			Island::Data data{ _island_data[i] };

			for (int id : data._island_ids_to_observe.cell)
			{
				if (id != -1)
				{
					_islands[id]._stone.addBeingLitObserver(&_islands[i]);
				}
			}

			_islands[i].addAudioEventObserver(&_audio_manager);
			_islands[i]._stone.addAudioEventObserver(&_audio_manager);
		}
	}
	
	// Walls.
	{
		uint wall_emissive_id{ _material_list.AddEmissive(1.0f) };
		uint gate_glass_id{ _material_list.AddGlass(1.3f, 50.0f) };

		// Create walls.
		for (int i = 0; i < _WALL_COUNT; ++i)
		{
			Wall::Data data{ _wall_data[i] };

			data.append(0xA2A2A2, 0xFF0000, non_metal_id, wall_emissive_id);

			// Initialize wall.
			switch (data._type)
			{
				case Wall::Type::BOUNDARY:
				{
					_walls[i].initialize(data);
					break;
				}
				case Wall::Type::GATE:
				{
					_walls[i].initialize(data, _gate_openings[data._gate_opening_index], gate_glass_id, "");
					break;
				}
				default:
					break;
			}

			// Build and store BVHs.
			_bvh_list.push_back(&(_walls[i]._bvh));
		};

		// Link observers.
		for (int i = 0; i < _WALL_COUNT; ++i)
		{
			Wall::Data data{ _wall_data[i] };

			if (data._island_id_to_observe != -1)
			{
				_islands[data._island_id_to_observe]._stone.addBeingLitObserver(&_walls[i]);
			}
		}
	}

	// Player ship.
	{
		// Update the cube size.
		uint3 size{ 4, 1, 1 };
		_player._ship._bvh.setCube(size);

		Cube& cube{ _player._ship._bvh._cube };

		// Voxel data.
		uint color{ 0xFF0000 };		

		// Set voxel data.
		for (int x = 0; x < 4; ++x)
		{
			color = (x == 3 ? 0x00FF00 : 0xFF0000);
			cube.set(x, 0, 0, non_metal_id, color);
		}

		// Prepare BVH.	
		_bvh_list.push_back(&_player._ship._bvh);
	}

	// Player avatar.	
	{
		// Update the cube size.
		uint3 size{ 1, 1, 1 };
		_player._avatar._bvh.setCube(size);

		Cube& cube{ _player._avatar._bvh._cube };

		// Voxel data.		
		uint color{ 0xFFFF00 };

		// Set voxel data.
		cube.set(0, 0, 0, non_metal_id, color);

		// Prepare BVH.
		_bvh_list.push_back(&_player._avatar._bvh);
	}

	// Player orb.
	{
		// Create inner light of orb.
		Light* orb_light{ _light_list.addAreaLightSphere({ 1.0f }, 0.0f, 0.0f, 0.0f) };		
		orb_light->_clamp_attenuation = true;

		// Get reference to orb material.
		Material& glass_material{ _material_list._materials[MaterialList::GetIndex(glass_id)] };

		// Initialize orb with material and light.
		Orb& orb{ _player._orb };

		orb.initialize(&glass_material, glass_id, orb_light);
		
		// Prepare BVH.
		_bvh_list.push_back(&orb._bvh);
	}

	// Prepare BVHs.
	for (size_t i = 0; i < _bvh_list.size(); ++i)
	{
		_bvh_list[i]->build();
		_bvh_list[i]->setID(static_cast<int>(i));
	}

	// Give all piers same id for identification.
	for (size_t i = 0; i < _piers.size(); ++i)
	{
		_piers[i]->setID(-9);
	}
	
	// Build TLAS.
	_tlas.build();

	// Initialize player.
	_player.initialize();
}


Scene::~Scene() {}


void Scene::update(const float delta_time, const KeyboardManager& km)
{
	switch (_stage)
	{
		case Stage::INTRO:
			updateIntro(delta_time, km);
			break;
		case Stage::WORLD:
			updateWorld(delta_time, km);
			break;
		default:
			break;
	}
}


void Scene::updateIntro(const float, const KeyboardManager&)
{

}


void Scene::updateWorld(const float delta_time, const KeyboardManager& km)
{
	_player.update(delta_time, km, this);

	for (Island& island : _islands)
	{
		island.update(delta_time);
	}
}


void Scene::refitAS()
{
	static uint rebuild_cycle{ 60u };
	static uint progress{ 0u };

	++progress;

	if (progress >= rebuild_cycle)
	{
		for (BVH* bvh : _bvh_list)
		{
			bvh->build();
		}

		_tlas.build();

		progress = 0u;
	}
	else
	{
		for (BVH* bvh : _bvh_list)
		{
			bvh->refitBVH();
		}

		_tlas.build();
	}
}


bool Scene::findNearest(Ray& ray) const
{
	_tlas.findNearest(ray, 0);
	
	// If t is less than max t, an intersection was found.
	return ray.t < Ray::t_max;
}


void Scene::findNearestToPlayer(Ray& player_ray, const int source_id) const
{
	_tlas.findNearestToPlayer(player_ray, 0, source_id);
}


// Unique traversal. For primitives, the answer is very easy.
// For cubes, we know the material exit exists within the same cube the ray entered the material in.
// So we can skip the TLAS/BLAS traversal and begin in the cube immediately.
void Scene::findMaterialExit(Ray& ray, const uint material_type) const
{
	// This method is called when a ray has a dielectric material as its source material
	// and it has hit the same dielectric material during trace()
	// Now it wants to find the way out of the material (no hollow materials).
	switch (ray._id)
	{
	case -2:		// Crossed the inner volume of the sphere and hit inner surface.
	{
		ray.normal *= -1.0f;	// Flip normal to get it pointing towards the center of sphere.
		ray._hit_data = 0;		// Assume air on outside of all spheres.		
		break;
	}
	case -1:		// Crossed the gap between parallel triangles and hit the next one.
	{
		ray._hit_data = 0;		// Assume air on outside of all triangles.
		break;
	}
	default:	// Hit the same voxel that it had entered. Must find where the non-material voxel is.
		_bvh_list[ray._id]->interesectItemForMaterialExit(ray, material_type);
		break;
	}
}


bool Scene::isOccluded(Ray& ray) const
{
	TintData dummy_tint_data{};

	return isOccluded(ray, dummy_tint_data);
}


bool Scene::isOccluded(Ray& ray, TintData& tint_data) const
{
	if (_tlas.findOcclusion(ray, tint_data, 0))
	{
		return true;
	}
	
	return false;
}


void Scene::eraseVoxels(Ray& ray)
{	
	_tlas.eraseVoxels(ray, 0, modified_cube);
}


void Scene::restoreVoxels()
{
	if (modified_cube)
	{
		modified_cube->restoreVoxelMemory();
		
		modified_cube = nullptr;
	}

}