#pragma once

namespace Tmpl8
{
	class Scene;
}


class LightList
{
public:
	// Ctor / Dtor.
	LightList(Scene* scene);

	~LightList() = default;


	// Methods.
	float3 getIllumination(const Ray& ray, const float3& surface_normal) const;
	float3 getIllumination(const Ray& ray, const float3& surface_normal, TintData& tint_data) const;

	Light* addDirectionalLight(float3 color, float intensity, float3 direction_to_world);
	Light* addPointLight(float3 color, float intensity, float3 position);
	Light* addSpotlight(float3 color, float intensity, float3 direction_to_world, float3 position, float falloff_factor = 0.5f, float cutoff_cos_theta = 0.8f);
	Light* addAreaLightRectangle(float3 color, float intensity, float3 direction_to_world, float3 position, float2 size);
	Light* addAreaLightSphere(float3 color, float intensity, float3 position, float radius);


	// Properties.
	std::vector<Light> _lights;
	float _max_range{ 0.0f };

	Scene* _scene;
};

