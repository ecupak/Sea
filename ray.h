#pragma once


#define USE_SSE 0


class Ray
{
public:
	// Ctors
	Ray() = default;
	Ray(const float3 origin, const float3 direction, const uint source_voxel, const bool apply_offset);
	Ray(const float3 origin, const float3 direction, const float rayLength, const bool apply_offset);
	Ray(const float3 origin, const float3 direction, const uint source_voxel, const float rayLength, const bool apply_offset);
	Ray(const float3 origin, const float3 direction, const float rayLength, const float offset_epsilon);


	// Methods.
	static float3 getAlbedo(const uint voxel);
	
	float3 IntersectionPoint() const { return O + t * D; }
	float3 GetNormal(const uint3& voxel_world_size) const;
	static void setEpsilon(const int epsilon_denominator);

	// Properties.
	static float _epsilon_offset;
	static int _epsilon_denominator;

	static constexpr float t_max = 20'000.0f;			// 1e34f;

#if USE_SSE
	Ray(const __m128 origin, const __m128 direction, const Ray& other);
#pragma warning ( push )
#pragma warning ( disable: 4201 /* nameless struct / union */ )
	union
	{
		struct
		{
			float3 O;								// ray origin
			uint _src_data;							// payload of the source voxel
		};
		__m128 O4;
	};
	
	union
	{
		struct
		{
			float3 D;								// ray direction			
			float t;								// ray length
		};
		__m128 D4;
	};

	union
	{
		struct
		{
			float3 rD;								// reciprocal ray direction			
			uint _hit_data;							// payload of the intersected voxel
		};
		__m128 rD4;
	};
#pragma warning ( pop )
#else
	float3 O;										// ray origin
	uint _src_data{ 0 };							// payload of the source voxel
	float3 D;										// ray direction			
	float t = t_max;								// ray length
	float3 rD;										// reciprocal ray direction			
	uint _hit_data{ 0 };							// payload of the intersected voxel
#endif
	float3 Dsign{ 1.0f };							// inverted ray direction signs, -1 or 1	
	float3 normal{ 1.0f };
	float _distance_underwater{ 0.0f };

	uint _dielectric_indicator{ 0 };
	int _id{ std::numeric_limits<int>().min() };	// id of the last cube ray was in - used to find material exits 

	inline void calculateDSign()
	{
		Dsign = (float3{
				static_cast<float>(*(uint*)&D.x >> 31) * 2 - 1,
				static_cast<float>(*(uint*)&D.y >> 31) * 2 - 1,
				static_cast<float>(*(uint*)&D.z >> 31) * 2 - 1 
			} + 1.0f) * 0.5f;
	};


	inline uint getGlassIndicator()
	{
		return _dielectric_indicator & 0x000000FF;
	}

	inline void setGlassIndicator(bool is_set)
	{
		_dielectric_indicator = (_dielectric_indicator & ~0x000000FF) | static_cast<uint>(is_set);
	}

	inline uint getWaterIndicator()
	{
		return (_dielectric_indicator >> 8) & 0x000000FF;
	}

	inline void setWaterIndicator(bool is_set)
	{
		_dielectric_indicator = (_dielectric_indicator & ~0x0000FF00) | (static_cast<uint>(is_set) << 8);
	}

private:
	// min3 is used in normal reconstruction.
	__inline static float3 min3(const float3& a, const float3& b)
	{
		return float3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
	}
};

