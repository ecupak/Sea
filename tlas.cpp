#include "precomp.h"
#include "tlas.h"


TLAS::TLAS(std::vector<BVH*>& bvh_list)
	: _blas{ bvh_list }
{	}


void TLAS::build()
{
	_blas_count = static_cast<int>(_blas.size());
	_nodes = static_cast<TLASNode*>MALLOC64(sizeof(TLASNode) * 2 * _blas_count);

	rebuild();
}


void TLAS::rebuild()
{
	// Assign a TLASleaf node to each BLAS
	int node_index[256];
	int node_indices{ _blas_count };

	_nodes_used = 1;

	for (int i = 0; i < _blas_count; ++i)
	{
		node_index[i] = _nodes_used;
		_nodes[_nodes_used]._aabb_min = _blas[i]->_bounds.bmin3;
		_nodes[_nodes_used]._aabb_max = _blas[i]->_bounds.bmax3;
		_nodes[_nodes_used]._blas_index = i;
		_nodes[_nodes_used++]._left_right = 0; // Makes it a leaf
	}

	// Use agglomerative clustering to build the TLAS.
	int A = 0;
	int B = findBestMatch(node_index, node_indices, A);

	while (node_indices > 1)
	{
		int C{ findBestMatch(node_index, node_indices, B) };

		if (A == C)
		{
			int node_index_A{ node_index[A] };
			int node_index_B{ node_index[B] };

			TLASNode& node_A = _nodes[node_index_A];
			TLASNode& node_B = _nodes[node_index_B];
			TLASNode& new_node = _nodes[_nodes_used];

			new_node._left_right = node_index_A + (node_index_B << 16);

			new_node._aabb_min = fminf(node_A._aabb_min, node_B._aabb_min);
			new_node._aabb_max = fmaxf(node_A._aabb_max, node_B._aabb_max);

			node_index[A] = _nodes_used++;
			node_index[B] = node_index[node_indices - 1];

			B = findBestMatch(node_index, --node_indices, A);
		}
		else
		{
			A = B;
			B = C;
		}
	}
	_nodes[0] = _nodes[node_index[A]];

}


int TLAS::findBestMatch(int* list, int N, int A)
{
	float smallest = 1e30f;

	int best_b{ -1 };

	for (int B = 0; B < N; ++B)
	{
		if (B != A)
		{
			// Find max and min of the proposed merge.
			float3 bmax{ fmaxf(_nodes[list[A]]._aabb_max, _nodes[list[B]]._aabb_max) };
			float3 bmin{ fminf(_nodes[list[A]]._aabb_min, _nodes[list[B]]._aabb_min) };

			// Find the area of the proposed merge.
			float3 e{ bmax - bmin };
			float surface_area{ (e.x * e.y) + (e.y * e.z) + (e.z * e.x) };

			// Store proposed merge candidate if it makes the best (smallest) surface area.
			if (surface_area < smallest)
			{
				smallest = surface_area;
				best_b = B;
			}
		}
	}

	return best_b;
}


void TLAS::findNearest(Ray& ray, const uint) const
{
	TLASNode* node_ptr{ &_nodes[0] };
	TLASNode* stack[64];
	uint stack_ptr{ 0 };

	while (true)
	{
		TLASNode& node{ *node_ptr };

		// Resolve leaf node.
		if (node._left_right == 0)
		{
			_blas[node._blas_index]->findNearest(ray, 0);

			if (stack_ptr == 0)
			{
				break;
			}
			else
			{
				node_ptr = stack[--stack_ptr];
			}

			continue;
		}

		// Check child nodes.
		TLASNode* child1{ &_nodes[node._left_right & 0xFFFF] };
		TLASNode* child2{ &_nodes[node._left_right >> 16] };

#if USE_SSE == 1
		float dist2 = BVH::intersectAABBForNearest_SSE(ray, child2->_aabb_min4, child2->_aabb_max4);
		float dist1 = BVH::intersectAABBForNearest_SSE(ray, child1->_aabb_min4, child1->_aabb_max4);
#else
		float dist1 = BVH::intersectAABBForNearest(ray, child1->_aabb_min, child1->_aabb_max);
		float dist2 = BVH::intersectAABBForNearest(ray, child2->_aabb_min, child2->_aabb_max);
#endif

		// Place closer child first for intersection resolution.
		if (dist1 > dist2)
		{
			swap(dist1, dist2);

			swap(child1, child2);
		}

		if (dist1 == Ray::t_max)
		{
			if (stack_ptr == 0)
			{
				break;
			}
			else
			{
				node_ptr = stack[--stack_ptr];
			}
		}
		else
		{
			node_ptr = child1;

			if (dist2 != Ray::t_max)
			{
				stack[stack_ptr++] = child2;
			}
		}
	}
}


bool TLAS::findOcclusion(Ray& ray, TintData& tint_data, const uint) const
{
	TLASNode* node_ptr{ &_nodes[0] };
	TLASNode* stack[64];
	uint stack_ptr{ 0 };

	while (true)
	{
		TLASNode& node{ *node_ptr };

		// Resolve leaf node.
		if (node._left_right == 0)
		{
			if (_blas[node._blas_index]->findOcclusion(ray, tint_data, 0))
			{
				return true;
			}

			if (stack_ptr == 0)
			{
				return false;
			}
			else
			{
				node_ptr = stack[--stack_ptr];
			}

			continue;
		}

		// Check child nodes.
		TLASNode* child1{ &_nodes[node._left_right & 0xFFFF] };
		TLASNode* child2{ &_nodes[node._left_right >> 16] };

#if USE_SSE == 1
		float dist2 = BVH::intersectAABBForNearest_SSE(ray, child2->_aabb_min4, child2->_aabb_max4);
		float dist1 = BVH::intersectAABBForNearest_SSE(ray, child1->_aabb_min4, child1->_aabb_max4);
#else
		float dist1 = BVH::intersectAABBForNearest(ray, child1->_aabb_min, child1->_aabb_max);
		float dist2 = BVH::intersectAABBForNearest(ray, child2->_aabb_min, child2->_aabb_max);
#endif

		// Place closer child first for intersection resolution.
		if (dist1 > dist2)
		{
			swap(dist1, dist2);

			swap(child1, child2);
		}

		if (dist1 > ray.t)
		{
			if (stack_ptr == 0)
			{
				return false;
			}
			else
			{
				node_ptr = stack[--stack_ptr];
			}
		}
		else
		{
			node_ptr = child1;

			if (dist2 < ray.t)
			{
				stack[stack_ptr++] = child2;
			}
		}
	}
}


void TLAS::findNearestToPlayer(Ray& ray, const uint, const int source_id) const
{
	TLASNode* node_ptr{ &_nodes[0] };
	TLASNode* stack[64];
	uint stack_ptr{ 0 };

	while (true)
	{
		TLASNode& node{ *node_ptr };

		// Resolve leaf node.
		if (node._left_right == 0)
		{
			_blas[node._blas_index]->findNearestToPlayer(ray, 0, source_id);

			if (stack_ptr == 0)
			{
				break;
			}
			else
			{
				node_ptr = stack[--stack_ptr];
			}

			continue;
		}

		// Check child nodes.
		TLASNode* child1{ &_nodes[node._left_right & 0xFFFF] };
		TLASNode* child2{ &_nodes[node._left_right >> 16] };

#if USE_SSE == 1
		float dist2 = BVH::intersectAABBForNearest_SSE(ray, child2->_aabb_min4, child2->_aabb_max4);
		float dist1 = BVH::intersectAABBForNearest_SSE(ray, child1->_aabb_min4, child1->_aabb_max4);
#else
		float dist1 = BVH::intersectAABBForNearest(ray, child1->_aabb_min, child1->_aabb_max);
		float dist2 = BVH::intersectAABBForNearest(ray, child2->_aabb_min, child2->_aabb_max);
#endif

		// Place closer child first for intersection resolution.
		if (dist1 > dist2)
		{
			swap(dist1, dist2);

			swap(child1, child2);
		}

		if (dist1 == Ray::t_max)
		{
			if (stack_ptr == 0)
			{
				break;
			}
			else
			{
				node_ptr = stack[--stack_ptr];
			}
		}
		else
		{
			node_ptr = child1;

			if (dist2 != Ray::t_max)
			{
				stack[stack_ptr++] = child2;
			}
		}
	}
}


void TLAS::eraseVoxels(Ray& ray, const uint, Cube*& modified_cube)
{
	TLASNode* node_ptr{ &_nodes[0] };
	TLASNode* stack[64];
	uint stack_ptr{ 0 };

	while (true)
	{
		TLASNode& node{ *node_ptr };

		// Resolve leaf node.
		if (node._left_right == 0)
		{
			_blas[node._blas_index]->eraseVoxels(ray, 0, modified_cube);

			if (stack_ptr == 0)
			{
				break;
			}
			else
			{
				node_ptr = stack[--stack_ptr];
			}

			continue;
		}

		// Check child nodes.
		TLASNode* child1{ &_nodes[node._left_right & 0xFFFF] };
		TLASNode* child2{ &_nodes[node._left_right >> 16] };

#if USE_SSE == 1
		float dist1 = BVH::intersectAABBForNearest_SSE(ray, child1->_aabb_min4, child1->_aabb_max4);
		float dist2 = BVH::intersectAABBForNearest_SSE(ray, child2->_aabb_min4, child2->_aabb_max4);
#else
		float dist1 = BVH::intersectAABBForNearest(ray, child1->_aabb_min, child1->_aabb_max);
		float dist2 = BVH::intersectAABBForNearest(ray, child2->_aabb_min, child2->_aabb_max);
#endif

		// Place closer child first for intersection resolution.
		if (dist1 > dist2)
		{
			swap(dist1, dist2);

			swap(child1, child2);
		}

		if (dist1 == Ray::t_max)
		{
			if (stack_ptr == 0)
			{
				break;
			}
			else
			{
				node_ptr = stack[--stack_ptr];
			}
		}
		else
		{
			node_ptr = child1;

			if (dist2 != Ray::t_max)
			{
				stack[stack_ptr++] = child2;
			}
		}
	}
}