#pragma once

// ALL TLAS/BLAS CODE HEAVILY INSPIRED (OR OUTRIGHT COPIED) FROM JACCO'S SERIES ON BVH CREATION
// [Credit] https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/


class BVH
{
public:
	// Public use structs.
	struct BVHNode
	{
#if USE_SSE
#pragma warning ( push )
#pragma warning ( disable: 4201 /* nameless struct / union */ )
		// Index of left node OR index of first primitive.
		union
		{
			struct
			{
				float3 _aabb_min;
				int _significant_index;
			};
			__m128 _aabb_min4;
		};

		// Number of children. Leaf if non-zero.
		union
		{
			struct
			{
				float3 _aabb_max;
				int _child_count;
			};
			__m128 _aabb_max4;
		};
#pragma warning ( pop )
#else
		float3 _aabb_min{ 0.0f };
		int _significant_index{ 0 };
		float3 _aabb_max{ 1.0f };
		int _child_count{ 0 };
#endif
	};


	// Ctors.
	BVH(const BVH&) = delete;
	BVH& operator=(const BVH&) = delete;
	BVH(BVH&&) = delete;
	BVH& operator=(BVH&&) = delete;

	// Dtor.
	virtual ~BVH() = default;

	// Vitual methods.
	virtual void build() = 0;
	virtual void refitBVH() const;
	virtual void setTransform(const float3& scale, const float3& rotation, const float3& translation);
	virtual void findNearest(Ray& ray, const uint node_index) const;
	virtual bool findOcclusion(Ray& ray, TintData& tint_data, const uint node_index) const;
	virtual void findNearestToPlayer(Ray& ray, const uint node_index, const int source_id) const;
	virtual void eraseVoxels(Ray& ray, const uint node_index, Cube*& modified_cube);
	virtual void setID(const int id);

	// Abstract methods.
	virtual void updateNodeBounds(uint node_index) const = 0;
	virtual void intersectItemForNearest(Ray& ray, const uint item_index) const = 0;
	virtual bool intersectItemForOcclusion(Ray& ray, TintData& tint_data, const uint item_index) const = 0;
	virtual void interesectItemForMaterialExit(Ray&, const uint) const {};
	virtual void intersectItemForNearestToPlayer(Ray& ray, const uint item_index, const int source_id) const = 0;
	virtual void intersectItemForVoxelErasure(Ray&, const uint, Cube*&) {}

	// Real method.
	void setBounds();

	// Statics methods.
	static float intersectAABBForNearest(const Ray& ray, const float3& bmin, const float3& bmax);	
	static float intersectAABBForNearest_SSE(const Ray& ray, const __m128 bmin4, const __m128 bmax4);
	static float calculateNodeCost(const BVHNode& node);

	aabb _bounds{ 0.0f, 1.0f };

	// Transformations.
	float3 _scale{ 1.0f };
	float3 _translate{ 0.0f };
	float3 _rotate{ 0.0f };

	// ID of BVH.
	int _id{ 0 };

	// Matrices.
	mat4 _matrix{ mat4::Identity() };
	mat4 _inverse_matrix{ _matrix.Inverted() };


protected:
	// Ctor.
	BVH() = default;

	// Internal use structs.
	struct Bin
	{
		aabb _bounds;
		int _count{ 0 };
	};

	// Ray transformation for transformed BVHs.
	inline Ray getTransformedRay(const Ray& ray) const
	{
#if USE_SSE
		Ray new_ray{
			TransformPosition_SSE(ray.O4, _inverse_matrix),
			TransformVector_SSE(ray.D4, _inverse_matrix),
			ray };
#else
		Ray new_ray{ ray };

		new_ray.O = TransformPosition(new_ray.O, _inverse_matrix);
		new_ray.D = TransformVector(new_ray.D, _inverse_matrix);
		new_ray.rD = float3{ 1.0f / new_ray.D }; // SIMD?
		new_ray.calculateDSign();
#endif

		return new_ray;
	}


	inline void transferDataToRay(const Ray& transformed_ray, Ray& ray) const
	{
		ray.t = transformed_ray.t;
		ray._hit_data = transformed_ray._hit_data;
		ray._id = transformed_ray._id;

		if (ray._id == _id)
		{
			ray.normal = normalize(TransformVector(transformed_ray.normal, _matrix));
		}
	}

	// Properites.
	int _root_node_index{ 0 };
	int _nodes_used{ 1 };
	BVHNode* _nodes{ nullptr };
	uint* _item_indicies{ nullptr };
};