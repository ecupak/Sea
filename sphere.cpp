#include "precomp.h"
#include "sphere.h"


Sphere::Sphere(const float3 position, const float radius)
	: _position{ position }
	, _radius{ radius }
{	}


Sphere::Sphere(const float3 position, const float radius, const uint material_data, const uint color)
	: _position{ position }
	, _radius{ radius }
	, _data{ material_data | color }
{	}


void Sphere::set(const uint material_data, const uint color)
{
	_data = material_data | color;
}