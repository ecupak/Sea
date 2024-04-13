#include "precomp.h"
#include "light.h"


// Point and Directional lights.
Light::Light(int id, LightType type, float3& color, float& intensity, float3& direction_to_world, float3& position)
	: _id{ id }
	, _type{ type }
	, _color{ color }
	, _intensity{ intensity }
	, _direction_to_world{ normalize(direction_to_world) }
	, _position{ position }
{	}


// Spotlights.
Light::Light(int id, LightType type, float3& color, float& intensity, float3& direction_to_world, float3& position, float& falloff_factor, float& cutoff_cos_theta)
	: _id{ id }
	, _type{ type }
	, _color{ color }
	, _intensity{ intensity }
	, _direction_to_world{ normalize(direction_to_world) }
	, _position{ position }
	, _falloff_factor{ falloff_factor }
	, _cutoff_cos_theta{ cutoff_cos_theta }
{	}


// Rectangular area lights.
Light::Light(int id, LightType type, float3& color, float& intensity, float3& direction_to_world, float3& position, float2& size)
	: _id{ id }
	, _type{ type }
	, _color{ color }
	, _intensity{ intensity }
	, _direction_to_world{ normalize(direction_to_world) }
	, _position{ position }
	, _size{ size }
{	
	calculateAreaRectOrientation();
}


// Spherical area lights.
Light::Light(int id, LightType type, float3& color, float& intensity, float3& position, float& radius)
	: _id{ id }
	, _type{ type }
	, _color{ color }
	, _intensity{ intensity }	
	, _position{ position }
	, _radius{ radius }
{	}


float3 Light::getIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const
{
	switch (_type)
	{
	case LightType::DIRECTIONAL:
		return getDirectionalIllumination(ray, surface_normal, scene, tint_data);
	case LightType::POINT:
		return getPointIllumination(ray, surface_normal, scene, tint_data);
	case LightType::SPOT:
		return getSpotIllumination(ray, surface_normal, scene, tint_data);	
	case LightType::AREA_RECT:
		return getAreaRectIllumination(ray, surface_normal, scene, tint_data);
	case LightType::AREA_SPHERE:
		return getAreaSphereIllumination(ray, surface_normal, scene, tint_data);
	default:
		assert(false && "Used light type enum that does not exist.");
		return { 0.0f };
	}
}


float3 Light::getDirectionalIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const
{
	// Prepare values.
	const float3 intersection_point{ ray.IntersectionPoint() };
	const float3 surface_to_light_normalized{ -_direction_to_world }; // direction_to_world is already normalized.

	// Angle to light.
	const float angle_of_incidence{ dot(surface_normal, surface_to_light_normalized) };
	if (angle_of_incidence <= 0.0f)
	{
		return { 0.0f };
	}

	// Get light.
	if (isVisible(intersection_point, -_direction_to_world, Ray::t_max, tint_data, scene))
	{
		float3 illumination{ _color * _intensity * angle_of_incidence };
		clampIllumination(illumination);
		return illumination;
	}
	
	return { 0.0f };
}


float3 Light::getPointIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const
{
	// Prepare values.
	const float3 intersection_point{ ray.IntersectionPoint() };
	const float3 surface_to_light_vector{ _position - intersection_point };
	const float surface_to_light_distance{ length(surface_to_light_vector) };
	const float3 surface_to_light_normalized{ surface_to_light_vector / surface_to_light_distance };

	// Angle to light.
	const float angle_of_incidence{ dot(surface_normal, surface_to_light_normalized) };
	if (angle_of_incidence <= 0.0f)
	{
		return { 0.0f };
	}

	// Get light.
	if (isVisible(intersection_point, surface_to_light_vector, surface_to_light_distance, tint_data, scene))
	{
		float3 illumination{ _color * _intensity * angle_of_incidence / (surface_to_light_distance * surface_to_light_distance) };
		clampIllumination(illumination);
		return illumination;
	}

	return { 0.0f };
}


float3 Light::getSpotIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const
{
	// Prepare values.
	const float3 intersection_point{ ray.IntersectionPoint() };
	const float3 surface_to_light_vector{ _position - intersection_point };
	const float surface_to_light_distance{ length(surface_to_light_vector) };
	const float3 surface_to_light_normalized{ surface_to_light_vector / surface_to_light_distance };

	// Angle to light.
	const float angle_of_incidence{ dot(surface_normal, surface_to_light_normalized) };
	if (angle_of_incidence <= 0.0f)
	{
		return { 0.0f };
	}

	// Angle to front of light.
	const float angle_of_alignment{ dot(_direction_to_world, -surface_to_light_normalized) };	
	if (angle_of_alignment < _cutoff_cos_theta)
	{
		return { 0.0f };
	}

	// Get light.
	if (isVisible(intersection_point, surface_to_light_vector, surface_to_light_distance, tint_data, scene))
	{		
		// Convert a range between 2 arbitrary values into [0, 1].
		// [Credit] OGLdev. https://ogldev.org/www/tutorial21/tutorial21.html
		float fall_off_intensity = 1.0f - (1.0f - angle_of_alignment) * (1.0f / (1.0f - _cutoff_cos_theta));

		float3 illumination{ _color * _intensity
			* fall_off_intensity * (_falloff_factor * _falloff_factor) // how quickly light decreases from the center.
			* angle_of_incidence / (surface_to_light_distance * surface_to_light_distance) // distance attenuation.
		};
		clampIllumination(illumination);
		return illumination;
	}

	return { 0.0f };
}


float3 Light::getAreaRectIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const
{
	// Choose random point on the area light.
	const float3 position_on_light{ _top_left + RandomFloat() * _u_vec + RandomFloat() * _v_vec };

	// Prepare values.
	const float3 intersection_point{ ray.IntersectionPoint() };
	const float3 surface_to_light_vector{ position_on_light - intersection_point };
	const float surface_to_light_distance{ length(surface_to_light_vector) };
	const float3 surface_to_light_normalized{ surface_to_light_vector / surface_to_light_distance };

	// Angle to light.
	const float angle_of_incidence{ dot(surface_normal, surface_to_light_normalized) };
	if (angle_of_incidence <= 0.0f)
	{
		return { 0.0f };
	}

	// Angle to front of light.
	const float angle_of_alignment{ dot(_direction_to_world, -surface_to_light_normalized) };
	if (angle_of_alignment <= 0.0f)
	{
		return { 0.0f };
	}

	// Get light.
	if (isVisible(intersection_point, surface_to_light_vector, surface_to_light_distance, tint_data, scene))
	{
		float3 illumination{ _color * _intensity * angle_of_incidence / (surface_to_light_distance * surface_to_light_distance) };
		clampIllumination(illumination);
		return illumination;
	}
	
	return { 0.0f };
}


void Light::calculateAreaRectOrientation()
{
	// _ahead will have been already set before calling this.
	float3 world_up{ 0.0f, 1.0f, 0.0f };
	float3 right{ normalize(cross(world_up, _direction_to_world)) };
	float3 up{ normalize(cross(_direction_to_world, right)) };

	// Find corners of area light rect.
	_top_left =_position - (right * _size.x * 0.5f) + (up * _size.y * 0.5f);
	float3 top_right{ _position + (right * _size.x * 0.5f) + (up * _size.y * 0.5f) };
	float3 bottom_left{ _position - (right * _size.x * 0.5f) - (up * _size.y * 0.5f) };

	// Calculate edges along the area light rect.
	_u_vec = top_right - _top_left;
	_v_vec = bottom_left - _top_left;
}


float3 Light::getAreaSphereIllumination(const Ray& ray, const float3& surface_normal, const Scene* scene, TintData& tint_data) const
{
	// Choose random point on the area light.
	const float3 position_on_light{ _position + _radius * randomUnitVector() };

	// Prepare values.
	const float3 intersection_point{ ray.IntersectionPoint() };
	const float3 surface_to_light_vector{ position_on_light - intersection_point };
	const float surface_to_light_distance{ length(surface_to_light_vector) };
	const float3 surface_to_light_normalized{ surface_to_light_vector / surface_to_light_distance };

	// Angle to light.
	const float angle_of_incidence{ dot(surface_normal, surface_to_light_normalized) };
	if (angle_of_incidence <= 0.0f)
	{
		return { 0.0f };
	}

	// Get light.
	if (isVisible(intersection_point, surface_to_light_vector, surface_to_light_distance, tint_data, scene))
	{
		float attenuation{ angle_of_incidence / (surface_to_light_distance * surface_to_light_distance) };
		//attenuation = _clamp_attenuation * fminf(attenuation, _max_attenuation) + !_clamp_attenuation * attenuation;
		if (_clamp_attenuation)
		{
			attenuation = fminf(attenuation, _max_attenuation);
		}
		float3 illumination{ _color * _intensity * attenuation };
		clampIllumination(illumination);
		return illumination;
	}

	return { 0.0f };
}


// [Credit] Lynn from Jacco's lecture.
void Light::clampIllumination(float3& illumination) const
{
	const float squaredMagnitude{ sqrLength(illumination) };

	if (squaredMagnitude > (_max_illumination_magnitude * _max_illumination_magnitude))
	{
		illumination = normalize(illumination) * _max_illumination_magnitude;
	}
}

const bool Light::isVisible(const float3& shadow_ray_origin, const float3& shadow_ray_direction, const float shadow_ray_length, TintData& tint_data, const Scene* scene) const
{
	Ray shadow_ray{ shadow_ray_origin, normalize(shadow_ray_direction), shadow_ray_length, true };

	return !scene->isOccluded(shadow_ray, tint_data);
}