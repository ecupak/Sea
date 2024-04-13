#include "precomp.h"
#include "orb.h"


void Orb::initialize(Material* material, uint material_data, Light* inner_light)
{
	// Store orb material so we can manipulate the intensity.
	_material = material;

	// Store orb material data so we can manipulate the color with set().
	_material_data = material_data;

	// Initialize color to clear.
	_bvh._sphere.set(material_data, 0xFFFFFF);

	// Store light.
	_inner_light = inner_light;

	// Set radius.
	_inner_light->_radius = _bvh._sphere._radius + 0.001f;
}


// When the avatar moves, move the orb to follow.
void Orb::applyOrientationAndTransform(float3& avatar_position, float3& up)
{
	static float minimum_offset{ VOXELSIZE * 2.0f };
	static float maximum_offset{ minimum_offset * 3.0f };

	_position = avatar_position + up * lerp(minimum_offset, maximum_offset, _intensity * inverse_max_intensity);
	
	_inner_light->_position = _position;

	_bvh.setTransform(_bvh._scale, _bvh._rotate, _position);
}


void Orb::update(const float delta_time)
{
	if (_is_charging)
	{
		_intensity += delta_time * _charge_speed;

		_intensity = clamp(_intensity, 0.0f, _max_intensity);

		_material->_absorption_intensity = _intensity;

		applyIntensityToLight();

		_is_charging = _intensity != _max_intensity;
	}
}


bool Orb::charge(uint charge_color)
{
	// Change to new color.
	if (charge_color != _color)
	{
		_color = charge_color;

		_bvh._sphere.set(_material_data, _color);
		_inner_light->_color = convertUintToFloat3(_color);
	}

	_is_charging = true;

	// Change to urgent music if had no charge.
	return _intensity == 0.0f;
}


bool Orb::drain(const float delta_time)
{
	if (!_is_charging && _intensity > 0.0f)
	{
		_intensity -= delta_time * _drain_speed;

		_intensity = fmaxf(0.0f, _intensity);

		applyIntensityToLight();

		// If just lost power, return true.
		return _intensity == 0.0f;
	}

	// Always return false unless just lost power.
	return false;
}


void Orb::applyIntensityToLight()
{
	// Update orb intensity.
	_material->_absorption_intensity = _intensity * 100.0f;

	// Update light intensity.
	float light_intensity = 0.0f;
	
	if (_intensity > 0.0f)
	{
		// Using exponential decay with positive slope.
		float _intensity_progress{ _intensity * inverse_max_intensity };
		
		float exponent{ lerp(0.0f, 8.0f, _intensity_progress) };

		light_intensity = -expf(-exponent) + 1.0f; // Exponential decay with positive slope.
	}

	_inner_light->_intensity = light_intensity * _max_light_intensity;
}


float3 Orb::convertUintToFloat3(uint color)
{
	int r = (color & 0x00'FF0000) >> 16;
	int g = (color & 0x00'00FF00) >> 8;
	int b = (color & 0x00'0000FF);
		
	static float inverse_color_max{ 1.0f / 256.0f };
	return make_float3(r, g, b) * inverse_color_max * 1.5f;
}