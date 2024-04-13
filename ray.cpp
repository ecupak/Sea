#include "precomp.h"
#include "ray.h"


// Static initializers.
float Ray::_epsilon_offset{ 0.00001f };
int Ray::_epsilon_denominator{ 4 };


Ray::Ray(const float3 origin, const float3 direction, const uint source_voxel, const bool apply_offset)
	: O{ apply_offset ? origin + direction * _epsilon_offset : origin }
	, D{ direction }
	, _src_data{ source_voxel }
	, t{ t_max }
{
#if USE_SSE
	rD4 = _mm_rcp_ps(D4);
#else
	rD = float3{ 1.0f / D.x, 1.0f / D.y, 1.0f / D.z };
#endif

	calculateDSign();
}


Ray::Ray(const float3 origin, const float3 direction, const float rayLength, const bool apply_offset)
	: O{ apply_offset ? origin + direction * _epsilon_offset : origin }
	, D{ direction }
	, t{ rayLength }
{
#if USE_SSE
	rD4 = _mm_rcp_ps(D4);
#else
	rD = float3{ 1.0f / D.x, 1.0f / D.y, 1.0f / D.z };
#endif

	calculateDSign();
}


Ray::Ray(const float3 origin, const float3 direction, const uint source_voxel, const float rayLength, const bool apply_offset)
	: O{ apply_offset ? origin + direction * _epsilon_offset : origin }
	, D{ direction }
	, t{ rayLength }
	, _src_data{ source_voxel }
{
#if USE_SSE
	rD4 = _mm_rcp_ps(D4);
#else
	rD = float3{ 1.0f / D.x, 1.0f / D.y, 1.0f / D.z };
#endif

	calculateDSign();
}


Ray::Ray(const float3 origin, const float3 direction, const float rayLength, const float offset_epsilon)
	: O{ origin + direction * offset_epsilon }
	, D{ direction }
	, t{ rayLength + offset_epsilon }
{
#if USE_SSE
	rD4 = _mm_rcp_ps(D4);
#else
	rD = float3{ 1.0f / D.x, 1.0f / D.y, 1.0f / D.z };
#endif

	calculateDSign();
}

#if USE_SSE
Ray::Ray(const __m128 origin, const __m128 direction, const Ray& other)
	: O4{ origin }
	, D4{ direction }
{
	rD4 = _mm_rcp_ps(D4);

	_src_data = other._src_data;
	t = other.t;
	_hit_data = other._hit_data;
	_id = other._id;
}
#endif


void Ray::setEpsilon(const int epsilon_denominator)
{
	_epsilon_denominator = epsilon_denominator;
	_epsilon_offset = 1.0f / pow(10.0f, static_cast<float>(epsilon_denominator));
}


float3 Ray::GetNormal(const uint3&) const
{
	// Return the voxel normal at the nearest intersection
	const float3 I1 = (O + t * D) * WORLDSIZE; // Scales ray decimal position to the current voxel world position.
	const float3 fG = fracf(I1);
	const float3 d = min3(fG, 1.0f - fG);
	const float mind = min(min(d.x, d.y), d.z);
	const float3 sign = Dsign * 2 - 1;
	
	return float3(mind == d.x ? sign.x : 0, mind == d.y ? sign.y : 0, mind == d.z ? sign.z : 0);
	
	// TODO:
	// - *only* in case the profiler flags this as a bottleneck:
	// - This function might benefit from SIMD.
}


float3 Ray::getAlbedo(const uint voxel)
{
	uint r{ (voxel & 0x00FF0000) >> 16 };
	uint g{ (voxel & 0x0000FF00) >> 8 };
	uint b{ (voxel & 0x000000FF) };

	static float inverse_max_value{ 1.0f / 255.0f };
	return float3(r * inverse_max_value, g * inverse_max_value, b * inverse_max_value);
}