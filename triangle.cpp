#include "precomp.h"
#include "triangle.h"


Triangle::Triangle(const float3 vertex_A, const float3 vertex_B, const float3 vertex_C)
	: _vertex_A{ vertex_A }
	, _vertex_B{ vertex_B }
	, _vertex_C{ vertex_C }
{	
	float inverse_3{ 1.0f / 3.0f };
	_centroid = float3{ (vertex_A + vertex_B + vertex_C) * inverse_3 };
}


Triangle::Triangle(const float3 vertex_A, const float3 vertex_B, const float3 vertex_C, const uint material_data, const uint color)
	: _vertex_A{ vertex_A }
	, _vertex_B{ vertex_B }
	, _vertex_C{ vertex_C }
	, _data{ material_data | color }
{
	float inverse_3{ 1.0f / 3.0f };
	_centroid = float3{ (vertex_A + vertex_B + vertex_C) * inverse_3 };
}


void Triangle::set(const uint material_data, const uint color)
{
	_data = material_data | color;
}