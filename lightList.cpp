#include "precomp.h"
#include "lightList.h"


LightList::LightList(Scene* scene)
	: _scene{ scene }
{	}


float3 LightList::getIllumination(const Ray& ray, const float3& surface_normal) const
{	
#ifdef _DEBUG
	if (_max_range == 0) return { 0.0f, 0.0f, 0.0f };
#endif

	TintData dummy_tint_data{};

	size_t light_index{ static_cast<size_t>(RandomFloat() * _max_range) };

	return _lights[light_index].getIllumination(ray, surface_normal, _scene, dummy_tint_data) * _max_range;
}


float3 LightList::getIllumination(const Ray& ray, const float3& surface_normal, TintData& tint_data) const
{
#ifdef _DEBUG
	if (_max_range == 0) return { 0.0f, 0.0f, 0.0f };
#endif

	size_t light_index{ static_cast<size_t>(RandomFloat() * _max_range) };

	return _lights[light_index].getIllumination(ray, surface_normal, _scene, tint_data) * _max_range;
}


Light* LightList::addDirectionalLight(float3 color, float intensity, float3 direction_to_world)
{
	float3 position{ 0.0f };

	_lights.emplace_back(static_cast<int>(_lights.size()), LightType::DIRECTIONAL, color, intensity, direction_to_world, position);
	_max_range = static_cast<float>(_lights.size());

	return &_lights.back();
}


Light* LightList::addPointLight(float3 color, float intensity, float3 position)
{
	float3 direction_to_world{ 0.0f };

	_lights.emplace_back(static_cast<int>(_lights.size()), LightType::POINT, color, intensity, direction_to_world, position);
	_max_range = static_cast<float>(_lights.size());

	return &_lights.back();
}


Light* LightList::addSpotlight(float3 color, float intensity, float3 direction_to_world, float3 position, float falloff_factor, float cutoff_cos_theta)
{
	_lights.emplace_back(static_cast<int>(_lights.size()), LightType::SPOT, color, intensity, direction_to_world, position, falloff_factor, cutoff_cos_theta);
	_max_range = static_cast<float>(_lights.size());

	return &_lights.back();
}


Light* LightList::addAreaLightRectangle(float3 color, float intensity, float3 direction_to_world, float3 position, float2 size)
{
	_lights.emplace_back(static_cast<int>(_lights.size()), LightType::AREA_RECT, color, intensity, direction_to_world, position, size);
	_max_range = static_cast<float>(_lights.size());

	return &_lights.back();
}


Light* LightList::addAreaLightSphere(float3 color, float intensity, float3 position, float radius)
{
	_lights.emplace_back(static_cast<int>(_lights.size()), LightType::AREA_SPHERE, color, intensity, position, radius);
	_max_range = static_cast<float>(_lights.size());

	return &_lights.back();
}