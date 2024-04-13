#pragma once


class Triangle
{
public:
	Triangle(const float3 vertex_A, const float3 vertex_B, const float3 vertex_C);
	Triangle(const float3 vertex_A, const float3 vertex_B, const float3 vertex_C, const uint material_data, const uint color);
	void set(const uint material_data, const uint color);

	float3 _vertex_A{ 0.0f };
	float3 _vertex_B{ 0.0f };
	float3 _vertex_C{ 0.0f };
	
	float3 _centroid{ 0.0f };
	
	uint _data{ 0 };
};

