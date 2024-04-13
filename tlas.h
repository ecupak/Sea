#pragma once

// ALL TLAS/BLAS CODE HEAVILY INSPIRED (OR OUTRIGHT COPIED) FROM JACCO'S SERIES ON BVH CREATION
// [Credit] https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/



struct TLASNode
{
#if USE_SSE
#pragma warning ( push )
#pragma warning ( disable: 4201 /* nameless struct / union */ )
	// Index of child.
	union
	{
		struct
		{
			float3 _aabb_min;
			int _blas_index;
		};
		__m128 _aabb_min4;
	};

	// Index of left and right child. Leaf if zero.
	union
	{
		struct
		{
			float3 _aabb_max;
			int _left_right;
		};
		__m128 _aabb_max4;
	};
#pragma warning ( pop )
#else
	float3 _aabb_min{ 0.0f };
	int _blas_index{ 0 };
	float3 _aabb_max{ 1.0f };
	int _left_right{ 0 };
#endif
};


class TLAS
{
public:
	TLAS(std::vector<BVH*>& bvh_list);


	void build();


	void rebuild();


	int findBestMatch(int* list, int N, int A);


	void findNearest(Ray& ray, const uint) const;


	bool findOcclusion(Ray& ray, TintData& tint_data, const uint) const;


	void findNearestToPlayer(Ray& ray, const uint, const int source_id) const;


	void eraseVoxels(Ray& ray, const uint, Cube*& modified_cube);


private:
	TLASNode* _nodes{ nullptr };
	uint _nodes_used{ 1 };
	std::vector<BVH*>& _blas;
	int _blas_count{ 0 };
};
