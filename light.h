#pragma once


namespace Tmpl8
{
	class Scene;
}


enum class LightType : uint
{
	DIRECTIONAL = 0,
	POINT,
	SPOT,
	AREA_RECT,
	AREA_SPHERE,
};


class Light
{
public:
	// Ctor / Dtor.
	Light(int id, LightType type, float3& color, float& intensity, float3& direction_to_world, float3& position);
	Light(int id, LightType type, float3& color, float& intensity, float3& direction_to_world, float3& position, float& falloff_factor, float& cutoff_cos_theta);
	Light(int id, LightType type, float3& color, float& intensity, float3& direction_to_world, float3& position, float2& size);
	Light(int id, LightType type, float3& color, float& intensity, float3& position, float& radius);
	~Light() = default;

	
	// Methods - all light types in 1 class.
	float3 getIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint) const;

	float3 getDirectionalIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const;
	float3 getPointIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const;
	float3 getSpotIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const;
	float3 getAreaRectIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const;
	float3 getAreaSphereIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const;

	void calculateAreaRectOrientation();


	// Properties (interleaved because computer likes float4 chunks).
	float3 _color{ 1.0f, 1.0f, 1.0f };
	float _intensity{ 1.0f }; // Because we use tonemapping, 0xFFFFFF ends up looking a bit dull / not very bright.
	float3 _direction_to_world{ 1.0f };
	float _falloff_factor{ 0.5 };
	float3 _position{ 1.0f };
	union
	{
		float _cutoff_cos_theta{ 0.8f };
		float _radius;
	};

	LightType _type{ LightType::DIRECTIONAL };
	int _id{ -1 };
	float2 _size{ 1.0f, 1.0f };
	float3 _top_left{ 1.0f };
	float3 _u_vec{ 1.0f };
	float3 _v_vec{ 1.0f };

	bool _clamp_attenuation{ false };
	float _max_attenuation{ 2.0f };
	float _max_illumination_magnitude{ 5.0f };

private:
	void clampIllumination(float3& illumination) const;
	const bool isVisible(const float3& shadow_ray_origin, const float3& shadow_ray_direction, const float shadow_ray_length, TintData& tint_data, const Scene* scene) const;
};

