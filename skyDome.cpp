#include "precomp.h"
#include "skyDome.h"

#include "stb_image.h"


SkyDome::SkyDome(const char* filename)
{
	_sky_pixels = stbi_loadf(filename, &_sky_width, &_sky_height, &_sky_bpp, 0);
		
	int max_index{ _sky_width * _sky_height * 3 };
	for (int i = 0; i < max_index; ++i)
	{
		_sky_pixels[i] = sqrtf(_sky_pixels[i]);
	}
}


float3 SkyDome::GetColor(const Ray& ray) const
{
	const float3 normalized_D{ normalize(ray.D) };

	uint u{ static_cast<uint>(_sky_width * atan2f(normalized_D.z, normalized_D.x) * INV2PI - 0.5f) };
	uint v{ static_cast<uint>(_sky_height * acosf(normalized_D.y) * INVPI - 0.5f) };

	static uint max_index{ static_cast<uint>(_sky_width * _sky_height) - 1u };
	uint sky_index{ clamp(u + v * _sky_width, 0u, max_index) };

	return float3(_sky_pixels[sky_index * 3], _sky_pixels[sky_index * 3 + 1], _sky_pixels[sky_index * 3 + 2]);
}