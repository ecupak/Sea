#include "precomp.h"
#include "wall.h"



void Wall::initialize(Data& data)
{
	createWallBVH(data);

	// Finalize.
	_bvh.build();

	// Place.	
	_offset = _bvh._bounds.bmax3 * 0.5f;
	_position = data._position;

	float3 translation{ _position - _offset };
	_bvh.setTransform(_bvh._scale, float3{ 0.0f, data._y_rotation, 0.0f }, translation);
}


void Wall::initialize(Data& data, int2 gate_opening, uint glass_id, const char*)
{
	createWallBVH(data);
	
	// Apply gate edits.
	_gate_opening = gate_opening;

	Cube& cube{ _bvh._cube };
	uint3& size{ cube._size };

	uint gate_width{ 1 + static_cast<uint>(_gate_opening.y * 2) };
	uint x_start{ static_cast<uint>(_gate_opening.x - _gate_opening.y) };

	uint gate_height{ static_cast<uint>(size.y * 0.5f) };
	uint y_start{ gate_height };
		
	for (uint y = y_start; y < size.y; ++y)
	{
		for (uint x = 0; x < gate_width; ++x)
		{
			for (uint z = 0; z < size.z; ++z)
			{
				cube.set(x + x_start, y, z, glass_id, 0xFFFF00);
			}
		}
	}

	// Finalize.
	_bvh.build();

	// Place.	
	_offset = _bvh._bounds.bmax3 * 0.5f;
	_position = data._position;

	float3 translation{ _position - _offset };
	float3 rotation{ 0.0f, data._y_rotation, 0.0f };
	_bvh.setTransform(_bvh._scale, rotation, translation);
}


void Wall::createWallBVH(Data& data)
{
	// Create cube.
	_bvh.setCube(data._size);

	// Create voxels.
	Cube& cube{ _bvh._cube };

	uint quarter_from_top{ static_cast<uint>(data._size.y - data._size.y * 0.25f) };
	uint quarter_from_bottom{ static_cast<uint>(data._size.y * 0.25f) };

	uint material{ 0 };
	uint color{ 0 };

	for (uint y = 0; y < data._size.y; ++y)
	{
		if (y == quarter_from_bottom)
		{
			material = data._emissive_id;
			color = data._stripe_color;
		}
		else if (y == quarter_from_top)
		{
			material = data._non_metal_id;
			color = data._stripe_color;
		}
		else
		{
			material = data._non_metal_id;
			color = data._wall_color;
		}

		for (uint x = 0; x < data._size.x; ++x)
		{
			for (uint z = 0; z < data._size.z; ++z)
			{
				cube.set(x, y, z, material, color);
			}
		}
	}
}


void Wall::onNotify(uint event_type, uint)
{
	if (event_type == EventType::ISLAND)
	{
		openGate();
	}
}


void Wall::openGate()
{
	Cube& cube{ _bvh._cube };
	uint3& size{ cube._size };

	uint x_start{ static_cast<uint>(_gate_opening.x - _gate_opening.y) };
	uint gate_width{ 1 + static_cast<uint>(_gate_opening.y * 2) };

	for (uint y = 0; y < size.y; ++y)
	{
		for (uint x = 0; x < gate_width; ++x)
		{
			for (uint z = 0; z < size.z; ++z)
			{
				cube.set(x + x_start, y, z, 0, 0);
			}
		}
	}
}