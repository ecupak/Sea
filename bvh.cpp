#include "precomp.h"
#include "bvh.h"


// TRAVERSAL METHODS //

void BVH::findNearest(Ray& ray, const uint node_index) const
{
	// Create a transformed ray to send into the BVH.
	Ray transformed_ray{ getTransformedRay(ray) };	

	// Trace transformed ray
	BVHNode* node_ptr{ &_nodes[node_index] };
	BVHNode* stack[64];
	uint stack_ptr = 0;

	while (true)
	{
		BVHNode& node{ *node_ptr };

		// Resolve leaf node.
		if (node._child_count > 0)
		{
			for (int i = 0; i < node._child_count; ++i)
			{
				intersectItemForNearest(transformed_ray, _item_indicies[node._significant_index + i]);
			}

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

		// Resolve child nodes.
		BVHNode* child1 = &_nodes[node._significant_index];
		BVHNode* child2 = &_nodes[node._significant_index + 1];

#if USE_SSE == 1
		float dist1 = intersectAABBForNearest_SSE(transformed_ray, child1->_aabb_min4, child1->_aabb_max4);
		float dist2 = intersectAABBForNearest_SSE(transformed_ray, child2->_aabb_min4, child2->_aabb_max4);
#else
		float dist1 = intersectAABBForNearest(transformed_ray, child1->_aabb_min, child1->_aabb_max);
		float dist2 = intersectAABBForNearest(transformed_ray, child2->_aabb_min, child2->_aabb_max);
#endif

		// Place closer child first for intersection resolution.
		if (dist1 > dist2)
		{
			swap(dist1, dist2); swap(child1, child2);
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

	// Move hit information into the original ray.
	transferDataToRay(transformed_ray, ray);
}


bool BVH::findOcclusion(Ray& ray, TintData& tint_data, const uint node_index) const
{
	// Create a transformed ray to send into the BVH.
	Ray transformed_ray{ getTransformedRay(ray) };

	// Trace transformed ray
	BVHNode* node_ptr{ &_nodes[node_index] };
	BVHNode* stack[64];
	uint stack_ptr = 0;

	while (true)
	{
		BVHNode& node{ *node_ptr };

		// Resolve leaf node.
		if (node._child_count > 0)
		{
			for (int i = 0; i < node._child_count; ++i)
			{
				if (intersectItemForOcclusion(transformed_ray, tint_data, _item_indicies[node._significant_index + i]))
				{
					return true;
				}
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

		// Resolve child nodes.
		BVHNode* child1 = &_nodes[node._significant_index];
		BVHNode* child2 = &_nodes[node._significant_index + 1];

#if USE_SSE == 1
		float dist2 = intersectAABBForNearest_SSE(transformed_ray, child2->_aabb_min4, child2->_aabb_max4);
		float dist1 = intersectAABBForNearest_SSE(transformed_ray, child1->_aabb_min4, child1->_aabb_max4);
#else
		float dist1 = intersectAABBForNearest(transformed_ray, child1->_aabb_min, child1->_aabb_max);
		float dist2 = intersectAABBForNearest(transformed_ray, child2->_aabb_min, child2->_aabb_max);
#endif

		// Place closer child first for intersection resolution.
		if (dist1 > dist2)
		{
			swap(dist1, dist2); swap(child1, child2);
		}

		if (dist1 == Ray::t_max)
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

			if (dist2 != Ray::t_max)
			{
				stack[stack_ptr++] = child2;
			}
		}
	}

	// Move hit information into the original ray.
	transferDataToRay(transformed_ray, ray);
}


void BVH::findNearestToPlayer(Ray& ray, const uint node_index, const int source_id) const
{
	// Create a transformed ray to send into the BVH.
	Ray transformed_ray{ getTransformedRay(ray) };

	// Trace transformed ray
	BVHNode* node_ptr{ &_nodes[node_index] };
	BVHNode* stack[64];
	uint stack_ptr = 0;

	while (true)
	{
		BVHNode& node{ *node_ptr };

		// Resolve leaf node.
		if (node._child_count > 0)
		{
			for (int i = 0; i < node._child_count; ++i)
			{
				intersectItemForNearestToPlayer(transformed_ray, _item_indicies[node._significant_index + i], source_id);
			}

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

		// Resolve child nodes.
		BVHNode* child1 = &_nodes[node._significant_index];
		BVHNode* child2 = &_nodes[node._significant_index + 1];

#if USE_SSE == 1
		float dist2 = intersectAABBForNearest_SSE(transformed_ray, child2->_aabb_min4, child2->_aabb_max4);
		float dist1 = intersectAABBForNearest_SSE(transformed_ray, child1->_aabb_min4, child1->_aabb_max4);
#else
		float dist1 = intersectAABBForNearest(transformed_ray, child1->_aabb_min, child1->_aabb_max);
		float dist2 = intersectAABBForNearest(transformed_ray, child2->_aabb_min, child2->_aabb_max);
#endif

		// Place closer child first for intersection resolution.
		if (dist1 > dist2)
		{
			swap(dist1, dist2); swap(child1, child2);
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

	// Move hit information into the original ray.
	transferDataToRay(transformed_ray, ray);
}


void BVH::eraseVoxels(Ray& ray, const uint node_index, Cube*& modified_cube)
{
	// Create a transformed ray to send into the BVH.
	Ray transformed_ray{ getTransformedRay(ray) };

	// Trace transformed ray
	BVHNode* node_ptr{ &_nodes[node_index] };
	BVHNode* stack[64];
	uint stack_ptr = 0;

	while (true)
	{
		BVHNode& node{ *node_ptr };

		// Resolve leaf node.
		if (node._child_count > 0)
		{
			for (int i = 0; i < node._child_count; ++i)
			{
				intersectItemForVoxelErasure(transformed_ray, 0, modified_cube);
			}

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

		// Resolve child nodes.
		BVHNode* child1 = &_nodes[node._significant_index];
		BVHNode* child2 = &_nodes[node._significant_index + 1];

#if USE_SSE == 1
		float dist2 = intersectAABBForNearest_SSE(transformed_ray, child2->_aabb_min4, child2->_aabb_max4);
		float dist1 = intersectAABBForNearest_SSE(transformed_ray, child1->_aabb_min4, child1->_aabb_max4);
#else
		float dist1 = intersectAABBForNearest(transformed_ray, child1->_aabb_min, child1->_aabb_max);
		float dist2 = intersectAABBForNearest(transformed_ray, child2->_aabb_min, child2->_aabb_max);
#endif

		// Place closer child first for intersection resolution.
		if (dist1 > dist2)
		{
			swap(dist1, dist2); swap(child1, child2);
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

	// Move hit information into the original ray.
	transferDataToRay(transformed_ray, ray);
}


float BVH::intersectAABBForNearest(const Ray& ray, const float3& bmin, const float3& bmax)
{	
	float tx1 = (bmin.x - ray.O.x) * ray.rD.x, tx2 = (bmax.x - ray.O.x) * ray.rD.x;
	float tmin = min(tx1, tx2), tmax = max(tx1, tx2);
	float ty1 = (bmin.y - ray.O.y) * ray.rD.y, ty2 = (bmax.y - ray.O.y) * ray.rD.y;
	tmin = max(tmin, min(ty1, ty2)), tmax = min(tmax, max(ty1, ty2));
	float tz1 = (bmin.z - ray.O.z) * ray.rD.z, tz2 = (bmax.z - ray.O.z) * ray.rD.z;
	tmin = max(tmin, min(tz1, tz2)), tmax = min(tmax, max(tz1, tz2));
	if (tmax >= tmin && tmin < ray.t && tmax > 0) return tmin; else return Ray::t_max;
}


#if USE_SSE
float BVH::intersectAABBForNearest_SSE(const Ray& ray, const __m128 bmin4, const __m128 bmax4)
{
	static __m128 mask4 = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_set_ps(1, 0, 0, 0));
	__m128 t1 = _mm_mul_ps(_mm_sub_ps(_mm_and_ps(bmin4, mask4), ray.O4), ray.rD4);
	__m128 t2 = _mm_mul_ps(_mm_sub_ps(_mm_and_ps(bmax4, mask4), ray.O4), ray.rD4);
	__m128 vmax4 = _mm_max_ps(t1, t2), vmin4 = _mm_min_ps(t1, t2);
	float tmax = min(vmax4.m128_f32[0], min(vmax4.m128_f32[1], vmax4.m128_f32[2]));
	float tmin = max(vmin4.m128_f32[0], max(vmin4.m128_f32[1], vmin4.m128_f32[2]));
	if (tmax >= tmin && tmin < ray.t && tmax > 0) return tmin; else return 1e30f;

#if 0
	// Make mask to hide last dummy value. _mm_set_ps is in reverse order.
	static __m128 mask4{ _mm_cmpeq_ps(_mm_setzero_ps(), _mm_set_ps(1, 0, 0, 0)) };

	/*	3 subs, 3 mults
		tx1 = (bmin.x - ray.O.x) * ray.rD.x
		ty1 = (bmin.y - ray.O.y) * ray.rD.y
		tz1 = (bmin.z - ray.O.z) * ray.rD.z
	*/
	__m128 t1{ _mm_mul_ps(_mm_sub_ps(_mm_and_ps(bmin4, mask4), ray.O4), ray.rD4) };

	/*	3 subs, 3 mults
		tx2 = (bmax.x - ray.O.x) * ray.rD.x
		ty2 = (bmax.y - ray.O.y) * ray.rD.y
		tz2 = (bmax.z - ray.O.z) * ray.rD.z
	*/
	__m128 t2{ _mm_mul_ps(_mm_sub_ps(_mm_and_ps(bmax4, mask4), ray.O4), ray.rD4) };

	/*	10 min/max
		tmin = max(max(min(tx1, tx2), min(ty1, ty2)), min(tz1, tz2))
		tmax = min(min(max(tx1, tx2), max(ty1, ty2)), max(tz1, tz2))
	*/
	__m128 vmin4{ _mm_min_ps(t1, t2) };
	__m128 vmax4{ _mm_max_ps(t2, t2) };

	// Moving from SSE back to human world. 24 operations turned into 8.
	float tmin{ max(vmin4.m128_f32[0], max(vmin4.m128_f32[1], vmin4.m128_f32[2])) };
	float tmax{ min(vmax4.m128_f32[0], min(vmax4.m128_f32[1], vmax4.m128_f32[2])) };

	if (tmax >= tmin && tmin < ray.t && tmax>0)
	{
		return tmin;
	}
	else
	{
		return Ray::t_max;
	}
#endif
}
#endif

// HELPER METHODS //

float BVH::calculateNodeCost(const BVHNode& node)
{
	const float3 extent{ node._aabb_max - node._aabb_min };
	const float area = (extent.x * extent.y) + (extent.y * extent.z) + (extent.z * extent.x);

	return node._child_count * area;
}


// MODIFY METHODS //

void BVH::setTransform(const float3& scale, const float3& rotate, const float3& translate)
{
	// Set new values.
	_scale = scale;
	_rotate = rotate;
	_translate = translate; // TODO: Use position instead.

	// Translate to pivot point.
	// TODO: Move half _size.
	float3 to_pivot_translation{ -0.5f, -0.5f, -0.5f };

	// Rotate around pivot point and return to original position.
	_matrix = mat4::Identity();

	_matrix = _matrix * mat4::Translate(_translate);
	_matrix = _matrix * mat4::Translate(-to_pivot_translation);
	_matrix = _matrix * mat4::RotateX(_rotate.x) * mat4::RotateY(_rotate.y) * mat4::RotateZ(_rotate.z);
	_matrix = _matrix * mat4::Scale(_scale);
	_matrix = _matrix * mat4::Translate(to_pivot_translation);

	_inverse_matrix = _matrix.Inverted();

	setBounds();
}


void BVH::setBounds()
{
	// Calculate world-space bounds using the new matrix.
	float3& bmin{ _nodes[0]._aabb_min };
	float3& bmax{ _nodes[0]._aabb_max };

	// Update bounding box.
	_bounds = aabb();

	for (int i = 0; i < 8; ++i)
	{
		_bounds.Grow(TransformPosition(float3{
			i & 1 ? bmax.x : bmin.x,
			i & 2 ? bmax.y : bmin.y,
			i & 4 ? bmax.z : bmin.z },
			_matrix
		));
	}
}


void BVH::refitBVH() const
{
	for (int i = _nodes_used - 1; i >= 0; i--)
	{
		if (i != 1)
		{
			BVHNode& node = _nodes[i];

			if (node._child_count > 0)
			{
				// Leaf node: adjust bounds to contained triangles
				updateNodeBounds(i);
			}
			else
			{
				// Interior node: adjust bounds to child node bounds
				BVHNode& left_child = _nodes[node._significant_index];
				BVHNode& right_child = _nodes[node._significant_index + 1];

				node._aabb_min = fminf(left_child._aabb_min, right_child._aabb_min);
				node._aabb_max = fmaxf(left_child._aabb_max, right_child._aabb_max);
			}
		}
	}
}


void BVH::setID(int)
{
	return;
}
