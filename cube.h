#pragma once


class Cube
{
public:
	struct DDAState
	{
		int3 step{ 0 };			// 16 bytes
		uint X{ 0 };
		uint Y{ 0 };
		uint Z{ 0 };			// 12 bytes
		float t{ 0.0f };		// 4 bytes
		float3 tdelta{ 0.0f };
		float dummy1{ 0 };		// 16 bytes
		float3 tmax{ 0.0f };
		float dummy2{ 0 };		// 16 bytes, 64 bytes in total
	};

	// Ctor.
	Cube();
	Cube(const uint3 size);

	~Cube() = default;

	void initialize(const uint3& size);

	// For non-BVH cubes controlled by Scene.
	float intersect(const Ray& ray) const;
	bool contains(const float3& pos) const;

	// Traversal methods.
	void findNearest(Ray& ray) const;
	bool findOcclusion(const Ray& ray, TintData& tint_data) const;
	void findMaterialExit(Ray& ray, const uint material_type) const;
	bool eraseVoxels(Ray& ray);
	void restoreVoxelMemory();

	// Modify methods.
	void set(uint x, uint y, uint z, uint material_data, uint voxel_color);
	
	// Properties.
	uint _id{ 0 };
	float3 _bounds[2]{ {0.0f}, {1.0f} };

	uint _pitch{ 64 };
	const float3 _position{ 0.0f };
	
	uint _slice{ 64 };
	uint3 _size{ 1 };

	unsigned int* _voxels{ nullptr };


private:
	struct VoxelMemory
	{
		VoxelMemory() = default;
		VoxelMemory(uint index, uint voxel)
			: _index{ index }
			, _voxel{ voxel }
		{	}

		uint _index{ 0 };
		uint _voxel{ 0 };
	};

	bool setup3DDDA(const Ray& ray, DDAState& state) const;
	void addToVoxelMemory(uint index, uint voxel);

	VoxelMemory _voxel_memory[1];
	uint _memory_index{ 0 };
};

