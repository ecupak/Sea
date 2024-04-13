#include "precomp.h"
#include "material.h"


Material::Material(float roughness, float ior, float absorption_intensity, float emissive_intensity)
	: _roughness{ roughness }
	, _index_of_refraction{ ior }
	, _absorption_intensity{ absorption_intensity }
	, _emissive_intensity{ emissive_intensity }
{	}


uint MaterialList::BITS_TO_INDEX{ 0 };
uint MaterialList::BITS_TO_TYPE{ 0 };

MaterialList::MaterialList()
{
	// Calculate bitshift amounts because I kept messing it up by hand.
	SetBitCount(MaterialMask::INDEX_MASK, BITS_TO_INDEX);
	SetBitCount(MaterialMask::TYPE_MASK, BITS_TO_TYPE);

	// Add air as the first "material". Needed for voxel glass math.
	Material air{ 0.0f, 1.0f, 0.0f, 0.0f };
	AddMaterial(air, MaterialType::AIR);
}


uint MaterialList::AddNonMetal(float roughness)
{
	Material material{ roughness, 1.0f, 0.0f, 0.0f };
	return AddMaterial(material, MaterialType::NON_METAL);
}


uint MaterialList::AddMetal(float roughness)
{
	Material material{ roughness, 1.0f, 0.0f, 0.0f };
	return AddMaterial(material, MaterialType::METAL);
}


uint MaterialList::AddGlass(float ior, float absorption_intensity)
{
	Material material{ 0.0f, ior, absorption_intensity, 0.0f };
	return AddMaterial(material, MaterialType::GLASS);
}


uint MaterialList::AddWater(float ior, float absorption_intensity)
{
	Material material{ 0.0f, ior, absorption_intensity, 0.0f };
	return AddMaterial(material, MaterialType::WATER);
}

uint MaterialList::AddBrightGlass(float ior, float absorption_intensity, float emissive_intensity)
{
	Material material{ 0.0f, ior, absorption_intensity, emissive_intensity };
	return AddMaterial(material, MaterialType::BRIGHT_GLASS);
}


uint MaterialList::AddEmissive(float emissive_intensity)
{
	Material material{ 0.0f, 1.0f, 0.0f, emissive_intensity };
	return AddMaterial(material, MaterialType::EMISSIVE);
}

uint MaterialList::AddMaterial(Material& material, uint material_type)
{
	_materials.push_back(material);

	return CreateBitflag(_materials.size() - 1, MaterialMask::INDEX_MASK) | CreateBitflag(material_type, MaterialMask::TYPE_MASK);
}


uint MaterialList::CreateBitflag(size_t value, MaterialMask mask)
{
	switch (mask)
	{
	case MaterialMask::INDEX_MASK:
		assert(value <= 0x0F && "Index is larger than allocated bits.");
		return static_cast<uint>(value << BITS_TO_INDEX);
	case MaterialMask::TYPE_MASK:
		return static_cast<uint>(value << BITS_TO_TYPE);
	default:
		return 0;
	}
}


void MaterialList::SetBitCount(uint mask, uint& bit_counter)
{
	for (bit_counter = 0; bit_counter < sizeof(mask) * 8; ++bit_counter)
	{
		uint value = mask & 0b1;

		if (value)
		{
			return;
		}

		mask >>= 1;
	}
}