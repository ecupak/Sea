#pragma once


class Sphere
{
public:
	Sphere(const float3 position, const float radius);
	Sphere(const float3 position, const float radius, const uint material_data, const uint color);
	void set(const uint material_data, const uint color);

	float3 _position{ 1.0f };
	float _radius{ 1.0f };
	float _inverse_radius{ 1.0f / _radius };
	uint _data{ 0 };
};
