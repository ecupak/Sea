#pragma once


struct TintData
{
	TintData() = default;

	uint _voxel{ 0 };
	float _distance{ 0.0f };
};


enum MaterialMask : uint
{
	INDEX_MASK	= 0xF0'00'00'00,
	TYPE_MASK	= 0x0F'00'00'00,
};


enum MaterialType : uint
{
	NON_METAL = 1,
	METAL,
	GLASS,
	WATER,
	BRIGHT_GLASS,
	EMISSIVE,
	COUNT,
	AIR = 0,
};


class Material
{
public:
	Material() = default;

	Material(float roughness, float ior, float absorption_intensity, float emissive_intensity);

	// Properties of the material.	
	float _roughness{ 0.0f };			// 4 bytes
	
	float _index_of_refraction{ 0.0f }; // 4 bytes

	float _emissive_intensity{ 1.0f };	// 4 bytes

	float _absorption_intensity{ 1.0f };// 4 bytes

	// = 16 bytes
};


class MaterialList
{
public:
	MaterialList();

	uint AddNonMetal(float roughness = 1.0f);
	uint AddMetal(float roughness = 0.0f);
	uint AddGlass(float ior, float absorption_intensity);
	uint AddWater(float ior, float absorption_intensity);
	uint AddBrightGlass(float ior, float absorption_intensity, float emissive_intensity);
	uint AddSmoke(float min_absorption_intensity, float max_absorption_intensity);
	uint AddEmissive(float emissive_intensity);

	Material& operator[](size_t index)
	{
		return _materials[index];
	}


	inline static uint GetIndex(uint voxel)
	{
		return (voxel & MaterialMask::INDEX_MASK) >> BITS_TO_INDEX;
	}


	inline static uint GetType(uint voxel)
	{
		return (voxel & MaterialMask::TYPE_MASK) >> BITS_TO_TYPE;
	}

	std::vector<Material> _materials; // Figure out how many materials will be used in game.
	// Then _align_malloc((number of material) * sizeof(Material), 64)

private:
	static void SetBitCount(uint mask, uint& bit_counter);
	
	uint AddMaterial(Material& material, uint material_type);
	uint CreateBitflag(size_t value, MaterialMask mask);

	static uint BITS_TO_INDEX;
	static uint BITS_TO_TYPE;
};