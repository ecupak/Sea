#include "precomp.h"
#include "cube.h"

Cube::Cube() {}


Cube::Cube(const uint3 size)
{
	initialize(size);
}


void Cube::initialize(const uint3& size)
{
	// Set size of cube and 3D indexing helpers.
	_size = size;
	_pitch = _size.x;
	_slice = _size.x * _size.y;
	
	// Create voxel grid.
	_voxels = static_cast<uint*>(MALLOC64(_slice * _size.z * sizeof(uint)));
	if (_voxels)
	{
		memset(_voxels, 0, _slice * _size.z * sizeof(uint));
	}

	// Set bounds.
	_bounds[0] = float3{ 0.0f };
	_bounds[1] = _bounds[0] + (make_float3(size) * VOXELSIZE);
}


// Set voxel data.
void Cube::set(const uint x, const uint y, const uint z, const uint material_data, const uint voxel_color)
{
	_voxels[x + y * _pitch + z * _slice] = material_data | voxel_color;
}


// Use to find intersection of a ray with the cube, only for rays that originate OUTSIDE the cube.
float Cube::intersect(const Ray& ray) const
{	
	const int sign_x = ray.rD.x < 0;
	const int sign_y = ray.rD.y < 0;
	const int sign_z = ray.rD.z < 0;

	float t_min = (_bounds[sign_x].x - ray.O.x) * ray.rD.x;
	float t_max = (_bounds[1 - sign_x].x - ray.O.x) * ray.rD.x;

	const float ty_min = (_bounds[sign_y].y - ray.O.y) * ray.rD.y;
	const float ty_max = (_bounds[1 - sign_y].y - ray.O.y) * ray.rD.y;

	if (t_min > ty_max || ty_min > t_max)
	{
		return Ray::t_max;
	}

	t_min = max(t_min, ty_min), t_max = min(t_max, ty_max);
	
	const float tz_min = (_bounds[sign_z].z - ray.O.z) * ray.rD.z;
	const float tz_max = (_bounds[1 - sign_z].z - ray.O.z) * ray.rD.z;
	
	if (t_min > tz_max || tz_min > t_max)
	{
		return Ray::t_max;
	}
	
	if ((t_min = max(t_min, tz_min)) > 0)
	{
		return t_min;
	}
	else
	{
		return Ray::t_max;
	}
}


// Test if position is inside the cube.
bool Cube::contains(const float3& position) const
{
	return position.x >= _bounds[0].x && position.y >= _bounds[0].y && position.z >= _bounds[0].z 
		&& position.x <= _bounds[1].x && position.y <= _bounds[1].y && position.z <= _bounds[1].z;
}


bool Cube::setup3DDDA(const Ray& ray, DDAState& state) const
{
	state.t = 0;

	if (!contains(ray.O))
	{
		state.t = intersect(ray);

		if (state.t >= Ray::t_max)
		{
			return false;
		}
	}

	// Get starting position of ray in cube, within the range of [gridsize, gridsize, gridsize] instead of [1, 1, 1].
	// Apply a small offset to nudge ray origin (starting point) into the cell it hit.
	// Use state.t as offset helper in case ray started outside the cube world and had to be advanced to the border.
	const float3 position_in_grid = WORLDSIZE * (ray.O + (state.t + Ray::_epsilon_offset) * normalize(ray.D));

	// Use this "real" position to find which cell the ray is starting in.
	const int3 P = clamp(make_int3(position_in_grid), 0, make_int3(_size) - 1);

	state.X = P.x;
	state.Y = P.y;
	state.Z = P.z;

	// Also called the direction, simply how the ray will march through the grid.
	// When tmax is advanced along an axis, the grid position (XYZ) will be incremented by the corresponding step value.
	state.step = make_int3(1 - ray.Dsign * 2);

	// Distance in each axis needed to get from 1 boundary to the next boundary.
	state.tdelta = VOXELSIZE * float3(state.step) * ray.rD;

	// Max distance traveled in each axis so far.
	// Always advance along the axis with the smallest tmax value.
	// Increment tmax by tdelta after advancing.
	// First (in case where the ray is starting inside a cell), find distance to start of cell
	//  and initialize tmax with that offset from the cell boundary.
	const float3 distance_from_boundary = (ceilf(position_in_grid) - ray.Dsign) * VOXELSIZE;
	state.tmax = (distance_from_boundary - ray.O) * ray.rD;

	// Begin traversal.
	return true;
}


void Cube::findNearest(Ray& ray) const
{
	// Setup Amanatides & Woo grid traversal
	DDAState s;
	if (!setup3DDDA(ray, s))
	{
		return;
	}

	// Start stepping.
	while (s.t <= ray.t)
	{
		const uint cell = _voxels[s.X + s.Y * _pitch + s.Z * _slice];

		if (cell)
		{
			if (s.t < ray.t)
			{
				ray.t = s.t;
				ray._hit_data = cell;
				ray._id = _id;
				ray.normal = ray.GetNormal(_size);
			}
			break;
		}

		if (s.tmax.x < s.tmax.y)
		{
			if (s.tmax.x < s.tmax.z)
			{
				s.t = s.tmax.x, s.X += s.step.x;

				if (s.X >= _size.x)
				{
					break;
				}

				s.tmax.x += s.tdelta.x;
			}
			else
			{
				s.t = s.tmax.z, s.Z += s.step.z;

				if (s.Z >= _size.z)
				{
					break;
				}

				s.tmax.z += s.tdelta.z;
			}
		}
		else
		{
			if (s.tmax.y < s.tmax.z)
			{
				s.t = s.tmax.y, s.Y += s.step.y;

				if (s.Y >= _size.y)
				{
					break;
				}

				s.tmax.y += s.tdelta.y;
			}
			else
			{
				s.t = s.tmax.z, s.Z += s.step.z;

				if (s.Z >= _size.z)
				{
					break;
				}

				s.tmax.z += s.tdelta.z;
			}
		}
	}
}


bool Cube::findOcclusion(const Ray& ray, TintData& tint_data) const
{
	// Setup Amanatides & Woo grid traversal
	DDAState s;
	if (!setup3DDDA(ray, s))
	{
		return false;
	}

	// Start stepping
	while (s.t < ray.t)
	{
		const uint cell = _voxels[s.X + s.Y * _pitch + s.Z * _slice];

		// Air and glass do not occlude.		
		{
			// Glass tints the incoming light.
			bool is_cell_glass{ MaterialList::GetType(cell) == MaterialType::GLASS };
			if (is_cell_glass)
			{
				// This is an approximation of Beer's law. I don't want to spend a lot of computing time here.
				// We'll assume we spend a distance of 1 in any glass hit by the shadow ray.
				tint_data._distance += 1.0f;

				// We'll also ignore the complexity of going through different glass colors.
				// Only the first glass hit (the closest to the surface sending the shadow ray) will be used.
				if (!tint_data._voxel)
				{
					tint_data._voxel = cell;
				}
			}

			if (!is_cell_glass && cell)
			{
				return s.t < ray.t;
			}
		}

		if (s.tmax.x < s.tmax.y)
		{
			if (s.tmax.x < s.tmax.z)
			{
				s.t = s.tmax.x, s.X += s.step.x;

				if (s.X >= _size.x)
				{
					break;
				}

				s.tmax.x += s.tdelta.x;
			}
			else
			{
				s.t = s.tmax.z, s.Z += s.step.z;

				if (s.Z >= _size.z)
				{
					break;
				}

				s.tmax.z += s.tdelta.z;
			}
		}
		else
		{
			if (s.tmax.y < s.tmax.z)
			{
				s.t = s.tmax.y, s.Y += s.step.y;

				if (s.Y >= _size.y)
				{
					break;
				}

				s.tmax.y += s.tdelta.y;
			}
			else
			{
				s.t = s.tmax.z, s.Z += s.step.z;

				if (s.Z >= _size.z)
				{
					break;
				}

				s.tmax.z += s.tdelta.z;
			}
		}
	}

	return false;
}


void Cube::findMaterialExit(Ray& ray, const uint material_type) const
{
	// Setup Amanatides & Woo grid traversal
	DDAState s;
	setup3DDDA(ray, s);

	// Start stepping.
	while (true)
	{
		const uint cell = _voxels[s.X + s.Y * _pitch + s.Z * _slice];

		if (MaterialList::GetType(cell) != material_type)
		{
			ray.t = s.t;
			ray._hit_data = cell;
			ray.normal = ray.GetNormal(_size);
			
			return;
		}

		if (s.tmax.x < s.tmax.y)
		{
			if (s.tmax.x < s.tmax.z)
			{
				s.t = s.tmax.x, s.X += s.step.x;

				if (s.X >= _size.x)
				{
					break;
				}

				s.tmax.x += s.tdelta.x;
			}
			else
			{
				s.t = s.tmax.z, s.Z += s.step.z;

				if (s.Z >= _size.z)
				{
					break;
				}

				s.tmax.z += s.tdelta.z;
			}
		}
		else
		{
			if (s.tmax.y < s.tmax.z)
			{
				s.t = s.tmax.y, s.Y += s.step.y;

				if (s.Y >= _size.y)
				{
					break;
				}

				s.tmax.y += s.tdelta.y;
			}
			else
			{
				s.t = s.tmax.z, s.Z += s.step.z;

				if (s.Z >= _size.z)
				{
					break;
				}

				s.tmax.z += s.tdelta.z;
			}
		}
	}

	// If no exit found, then ray reached end of voxel volume.
	// We will assume there is air on the outside.
	ray.t = s.t;
	ray._hit_data = 0;
	ray.normal = ray.GetNormal(_size);
}


bool Cube::eraseVoxels(Ray& ray)
{
	bool was_killed{ false };

	// Setup Amanatides & Woo grid traversal
	DDAState s;
	if (!setup3DDDA(ray, s))
	{
		return false;
	}

	// Start stepping.
	while (s.t <= ray.t)
	{
		uint index = s.X + s.Y * _pitch + s.Z * _slice;
		uint& cell = _voxels[index];

		if (cell)
		{
			// Return that this cube was modified.
			was_killed = true;
			
			// Store the voxel location and data.
			addToVoxelMemory(index, cell);

			// Temporarily "erase" voxel (make air).
			cell = 0;
		}

		if (s.tmax.x < s.tmax.y)
		{
			if (s.tmax.x < s.tmax.z)
			{
				s.t = s.tmax.x, s.X += s.step.x;

				if (s.X >= _size.x)
				{
					break;
				}

				s.tmax.x += s.tdelta.x;
			}
			else
			{
				s.t = s.tmax.z, s.Z += s.step.z;

				if (s.Z >= _size.z)
				{
					break;
				}

				s.tmax.z += s.tdelta.z;
			}
		}
		else
		{
			if (s.tmax.y < s.tmax.z)
			{
				s.t = s.tmax.y, s.Y += s.step.y;

				if (s.Y >= _size.y)
				{
					break;
				}

				s.tmax.y += s.tdelta.y;
			}
			else
			{
				s.t = s.tmax.z, s.Z += s.step.z;

				if (s.Z >= _size.z)
				{
					break;
				}

				s.tmax.z += s.tdelta.z;
			}
		}
	}

	return was_killed;
}


void Cube::addToVoxelMemory(uint index, uint voxel)
{
	_voxel_memory[_memory_index++] = VoxelMemory{index, voxel};
}


void Cube::restoreVoxelMemory()
{
	for (uint i = 0; i < _memory_index; ++i)
	{
		auto [index, voxel] = _voxel_memory[i];

		_voxels[index] = voxel;
	}

	_memory_index = 0;
}