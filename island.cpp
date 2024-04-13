#include "precomp.h"
#include "island.h"


void Island::initialize(int id, Data& data, uint non_metal_id, uint other_id)
{
	// Set data.
	_id = id;
	_location = data._location;
	_color = data._island_color;	
	_observed_ids = data._island_ids_to_observe;
	
	// Create BVHs.
	createLandBVH(data, non_metal_id);
	createPierBVH(data, non_metal_id);

	// Create stone.
	float up{ _location == Location::ABOVE || _location == Location::SINK ? 1.0f : -1.0f };
	_stone.initialize(_id, data._stone_type, data._stone_color, non_metal_id, other_id, up, *this);
}


void Island::createLandBVH(Data& data, uint non_metal_id)
{
	// Size.
	int height = 10;
	int radius = 8;
	int diameter_width = radius * 2;

	uint3 size{ make_uint3(diameter_width, height, diameter_width) };

	// Create cube.
	_bvh.setCube(size);

	// Create voxels.
	Cube& cube{ _bvh._cube };

	int x_origin = radius;	// Width and depth are both diameter, so start in center.
	int z_origin = radius;

	int radius_squared{ radius * radius };

	for (int z = 1; z < diameter_width; ++z)
	{
		int z_distance{ z_origin - z };

		for (int x = 1; x < diameter_width; ++x)
		{
			int x_distance{ x_origin - x };

			if (int distance_from_center{ z_distance * z_distance + x_distance * x_distance };
				distance_from_center <= radius_squared)
			{
				for (int y = 0; y < height; ++y)
				{
					cube.set(x, y, z, non_metal_id, _color);
				}
			}
		}
	}

	// Finalize.
	_bvh.build();

	// Place.
	float y_position{ 0.5f };
	float offset_to_surface{ (static_cast<float>(height) * 0.5f) - 1.0f };
	switch (_location)
	{
		case Location::ABOVE:
		{
			y_position = 0.5f - (offset_to_surface * VOXELSIZE);
			break;
		}
		case Location::BELOW:
		{
			y_position = 0.5f + (offset_to_surface * VOXELSIZE);
			break;
		}
		case Location::RISE: // Starts mid and becomes like BELOW.
		{
			y_position = 0.5f;
			break;
		}
		case Location::SINK: // Starts mid and becomes like ABOVE.
		{
			y_position = 0.5f;
			break;
		}
		default:
			break;
	}

	_offset = _bvh._bounds.bmax3 * 0.5f;
	_position = { data._position.x, y_position, data._position.y };

	float3 translation{ _position - _offset };	
	float y_rot{ atan2f(static_cast<float>(data._facing.y), static_cast<float>(data._facing.x)) };
	float3 rotation{ 0.0f, y_rot , 0.0f };
	_bvh.setTransform(_bvh._scale, rotation, translation);
}


void Island::createPierBVH(Data& data, uint non_metal_id)
{
	// Size.
	uint3 size{ 5, 1, 3 };

	// Create cube.
	_pier._bvh.setCube(size);
	Cube& cube{ _pier._bvh._cube };

	// Create voxels.
	uint wood_color{ 0xA1662F };

	uint y = 0;
	for (uint x = 0; x < size.x; ++x)
	{
		for (uint z = 0; z < size.z; ++z)
		{
			cube.set(x, y, z, non_metal_id, wood_color);
		}
	}

	// Finalize.
	_pier._bvh.build();

	// Place.
	_pier._offset = _pier._bvh._bounds.bmax3 * 0.5f;

	float up{ 0.0f };
	switch (_location)
	{
		case Location::ABOVE:
		case Location::SINK:
		{
			up = 1.0f;
			break;
		}
		case Location::BELOW:
		case Location::RISE:
		{
			up = -1.0f;
			break;
		}
		default:
			break;
	}

	_pier._position = float3{
		_position.x + data._facing.x * (_offset.x + _pier._offset.x) + (data._facing.x != 0 ? data._facing.y * VOXELSIZE : 0.0f),
		0.5f + VOXELSIZE * 0.25001f * up,
		// Z component, unfortunately using the default y component name of a 2-vector variable.
		_position.z + data._facing.y * (_offset.z + _pier._offset.x) + (data._facing.y != 0 ? -VOXELSIZE : 0.0f)
	};

	float3 scale{ 1.0f, 0.5f, 1.0f };
	float3 translation{ _pier._position - _pier._offset };
	_pier._bvh.setTransform(scale, _bvh._rotate, translation);
}


void Island::update(const float delta_time)
{
	if (_is_moving)
	{
		static float initial_island_y{ _position.y };
		static float initial_stone_y{ _stone._position.y };

		// Distance to move the island * direction to move it.
		static float delta_y { (_offset.y - VOXELSIZE) * (_location == Location::SINK ? -1.0f : 1.0f) };

		static float t{ 0.0f };

		// Move island.
		{
			_is_moving = t < 1.0f;

			_position.y = lerp(initial_island_y, initial_island_y + delta_y, t);
			_stone._position.y = lerp(initial_stone_y, initial_stone_y + delta_y, t);

			t += delta_time * 0.1f;

			// Move island and all things "attached" to it. The island shakes.
			float3 translation{ _position - _offset + float3{ VOXELSIZE * 0.5f * RandomFloat(), 0.0f, VOXELSIZE * 0.5f * RandomFloat() } };
			_bvh.setTransform(_bvh._scale, _bvh._rotate, translation);

			translation = _stone._position - _stone._offset;
			_stone._bvh.setTransform(_stone._bvh._scale, _stone._bvh._rotate, translation);
		}

		if (!_is_moving)
		{
			_stone._surface = _position.y + _offset.y;
			_on_audio_event.notify(EventType::SFX, SFXEvent::ON_ISLAND_STOP);
		}
	}

	_stone.update(delta_time);	
}


void Island::addAudioEventObserver(Observer* observer)
{
	_on_audio_event.addObserver(observer);
}


void Island::onNotify(uint, uint event_id)
{
	int i_event_id{ static_cast<int>(event_id) };
	if (i_event_id == _observed_ids[0])
	{
		switch (_location)
		{
			case Location::RISE:
			case Location::SINK:
			{
				_is_moving = true;
				_on_audio_event.notify(EventType::SFX, SFXEvent::ON_ISLAND_MOVE);
				break;
			}
		}
	}
	else if (i_event_id == _observed_ids[1])
	{
		_stone._was_light_delivered = true;
	}
}