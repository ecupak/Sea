#pragma once


class Model
{
public:
	float3 _position { 1.0f };
	float3 _offset { 0.0f };
	CubeBVH _bvh;
};