#pragma once

// [Credit] Jacco https://jacco.ompf2.com/2022/05/27/how-to-build-a-bvh-part-8-whitted-style/
class SkyDome
{
public:
	SkyDome(const char* filename);

	float3 GetColor(const Ray& ray) const;

private:
	int _sky_width{ 0 };
	int _sky_height{ 0 };
	int _sky_bpp{ 0 };

	float* _sky_pixels{ nullptr };
};