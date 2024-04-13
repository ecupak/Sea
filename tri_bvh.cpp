#include "precomp.h"
#include "tri_bvh.h"

TriBVH::TriBVH(std::vector<Triangle>& triangles)
	: _triangles{ triangles }
{	
	_id = -1;
}


TriBVH::~TriBVH()
{
	delete[] _nodes;
	delete[] _item_indicies;
}


// INTERSECT ITEM METHODS //

void TriBVH::intersectItemForNearest(Ray& transformed_ray, const uint item_index) const
{
	const static float epsilon{ 0.0001f };

	Triangle& tri{ _triangles[item_index] };

	float3 ray_dir_normalized{ normalize(transformed_ray.D) };

	const float3 A_to_B{ tri._vertex_B - tri._vertex_A };
	const float3 A_to_C{ tri._vertex_C - tri._vertex_A };

	const float3 h{ cross(ray_dir_normalized, A_to_C) };
	const float a{ dot(A_to_B, h) };

	if (a > -epsilon && a < epsilon)
	{
		// Ray is parallel to triangle.
		return;
	}

	const float inverse_a{ 1.0f / a };

	const float3 s{ transformed_ray.O - tri._vertex_A };
	const float u{ inverse_a * dot(s, h) };

	if (u < 0 || u > 1)
	{
		return;
	}

	const float3 q = cross(s, A_to_B);
	const float v = inverse_a * dot(ray_dir_normalized, q);

	if (v < 0 || u + v > 1)
	{
		return;
	}

	const float t = inverse_a * dot(A_to_C, q);

	if (t > epsilon && t < transformed_ray.t)
	{
		float3 normal = normalize(cross(A_to_C, A_to_B));
		float cos_theta = dot(-ray_dir_normalized, normal);
		normal *= (cos_theta > 0.0f ? 1.0f : -1.0f);

		transformed_ray.t = t;
		transformed_ray.normal = normal;
		transformed_ray._hit_data = tri._data;
		transformed_ray._id = _id;
	}
}


bool TriBVH::intersectItemForOcclusion(Ray& transformed_ray, TintData& tint_data, const uint item_index) const
{
	Triangle& tri{ _triangles[item_index] };

	float3 ray_dir_normalized{ normalize(transformed_ray.D) };

	const static float epsilon{ 0.0001f };

	const float3 edge1{ tri._vertex_B - tri._vertex_A };
	const float3 edge2{ tri._vertex_C - tri._vertex_A };

	const float3 h{ cross(ray_dir_normalized, edge2) };
	const float a{ dot(edge1, h) };

	if (a > -epsilon && a < epsilon)
	{
		// Ray is parallel to triangle.
		return false;
	}

	const float inverse_a{ 1.0f / a };

	const float3 s{ transformed_ray.O - tri._vertex_A };
	const float u{ inverse_a * dot(s, h) };

	if (u < 0 || u > 1)
	{
		return false;
	}

	const float3 q = cross(s, edge1);
	const float v = inverse_a * dot(ray_dir_normalized, q);

	if (v < 0 || u + v > 1)
	{
		return false;
	}

	const float t = inverse_a * dot(edge2, q);

	if (t > epsilon)
	{
		const bool is_cell_glass{ MaterialList::GetType(tri._data) == MaterialType::GLASS };
		
		// Glass does not occlude.		
		if (is_cell_glass)
		{
			// We'll ignore the complexity of going through different glass colors.
			// Only the first glass hit (the closest to the surface sending the shadow ray) will be used.
			if (tint_data._voxel == 0)
			{
				// This is an approximation for Beer's law. I don't want to spend a lot of computing time here.
				// We'll assume we travel, on average, more than a direct line to the other side of the triangle "box" of gap 1.
				tint_data._distance += 1.5f;
				tint_data._voxel = tri._data;
			}
		}

		const bool is_cell_water{ MaterialList::GetType(tri._data) == MaterialType::WATER };
	
		return !(is_cell_glass || is_cell_water);
	}

	return false;
}


void TriBVH::intersectItemForNearestToPlayer(Ray& ray, const uint item_index, const int source_id) const
{
	// Camera obstruction ray ignores water during flip.
	if (source_id == _id)
	{
		return;
	}

	intersectItemForNearest(ray, item_index);
}


// BUILD METHODS //

void TriBVH::build()
{
	Timer t;

	delete[] _nodes;
	delete[] _item_indicies;
	_nodes_used = 1;

	// Set primitive count and create all nodes that will be used.
	const int N{ static_cast<int>(_triangles.size()) };
	_nodes = new BVHNode[(N * 2) - 1];
	_item_indicies = new uint[N];

	// Build out primitive indicies array.
	for (int i = 0; i < N; ++i)
	{
		_item_indicies[i] = i;
	}

	BVHNode& root{ _nodes[_root_node_index] };
	root._significant_index = 0;
	root._child_count = N;

	updateNodeBounds(0);
	subdivide(0);
	setBounds();
}


void TriBVH::updateNodeBounds(uint node_index) const
{
	BVHNode& node{ _nodes[node_index] };

	node._aabb_min = float3{ 1e30f };
	node._aabb_max = float3{ -1e30f };

	// Check all primitives to get the smallest and largest point.
	for (int i = 0; i < node._child_count; ++i)
	{
		const uint tri_index{ _item_indicies[node._significant_index + i] };
		Triangle& tri{ _triangles[tri_index] };

		node._aabb_min = fminf(node._aabb_min, tri._vertex_A);
		node._aabb_min = fminf(node._aabb_min, tri._vertex_B);
		node._aabb_min = fminf(node._aabb_min, tri._vertex_C);

		node._aabb_max = fmaxf(node._aabb_max, tri._vertex_A);
		node._aabb_max = fmaxf(node._aabb_max, tri._vertex_B);
		node._aabb_max = fmaxf(node._aabb_max, tri._vertex_C);
	}
}


void TriBVH::subdivide(const uint node_index)
{
	// Get node to split.
	BVHNode& node{ _nodes[node_index] };

	// Abort subdivision / end recursion if contains too few primitives (becomes leaf).
	if (node._child_count < 2)
	{
		return;
	}

	// Find split position.
	int axis{ -1 };
	float split_position{ 0.0f };
	{
		int best_axis{ 0 };
		float best_position{ 0.0f };
		const float split_cost{ findBestSplitPlane(node, /*inout*/ best_axis, /*inout*/ best_position) };
		const float no_split_cost{ calculateNodeCost(node) };

		if (split_cost >= no_split_cost)
		{
			return;
		}
		else
		{
			axis = best_axis;
			split_position = best_position;
		}
	}

	// Split the BVH in half. Swap elements so they are consecutive within their new container. No sorting needed beyond that.
	int left_i{ node._significant_index };
	int right_i{ left_i + node._child_count - 1 };

	while (left_i <= right_i)
	{
		if (_triangles[left_i]._centroid[axis] < split_position)
		{
			++left_i;
		}
		else
		{
			std::swap(_triangles[left_i], _triangles[right_i--]);
		}
	}

	// Value of left_i is amount of primitives on the lesser (left) side of the split, offset by the first_primitive in the node.
	const int left_count{ left_i - node._significant_index };
	if (left_count == 0 || left_count == node._child_count)
	{
		return;
	}

	// Create new child nodes.
	const int left_child_index{ _nodes_used++ };
	_nodes[left_child_index]._significant_index = node._significant_index;
	_nodes[left_child_index]._child_count = left_count;

	const int right_child_index{ _nodes_used++ };
	_nodes[right_child_index]._significant_index = left_i;
	_nodes[right_child_index]._child_count = node._child_count - left_count;

	// Update this node's data.
	node._significant_index = left_child_index;
	node._child_count = 0;

	// Update and split children.
	updateNodeBounds(left_child_index);
	updateNodeBounds(right_child_index);

	subdivide(left_child_index);
	subdivide(right_child_index);
}


float TriBVH::findBestSplitPlane(BVHNode& node, int& best_axis, float& best_position) const
{
	constexpr static int splits{ 8 };

	float best_cost{ 1e30f };

	for (int axis = 0; axis < 3; ++axis)
	{
		// Find min/max bounds to make splits between.
		float bounds_min = 1e30f;
		float bounds_max = -1e30f;
		{
			for (int i = 0; i < node._child_count; ++i)
			{
				Triangle& tri{ _triangles[_item_indicies[node._significant_index + i]] };

				bounds_min = min(bounds_min, tri._centroid[axis]);
				bounds_max = max(bounds_max, tri._centroid[axis]);
			}

			// If flat, skip.
			if (bounds_min == bounds_max)
			{
				continue;
			}
		}

		// BIN creation.
		Bin bins[splits];
		{
			float scale{ splits / (bounds_max - bounds_min) };

			for (int i = 0; i < node._child_count; ++i)
			{
				Triangle& tri{ _triangles[_item_indicies[node._significant_index + i]] };

				int bin_index{ min(splits - 1, static_cast<int>((tri._centroid[axis] - bounds_min) * scale)) };
				bins[bin_index]._count++;
				bins[bin_index]._bounds.Grow(tri._vertex_A);
				bins[bin_index]._bounds.Grow(tri._vertex_B);
				bins[bin_index]._bounds.Grow(tri._vertex_C);
			}
		}

		// Plane setup.
		int left_count[splits - 1];
		int right_count[splits - 1];
		float left_area[splits - 1];
		float right_area[splits - 1];
		{
			aabb left_box;
			aabb right_box;

			int left_sum{ 0 };
			int right_sum{ 0 };

			for (int i = 0; i < splits - 1; ++i)
			{
				left_sum += bins[i]._count;
				left_count[i] = left_sum;
				left_box.Grow(bins[i]._bounds);
				left_area[i] = left_box.Area();

				right_sum += bins[splits - 1 - i]._count;
				right_count[splits - 2 - i] = right_sum;
				right_box.Grow(bins[splits - 1 - i]._bounds);
				right_area[splits - 2 - i] = right_box.Area();
			}
		}

		// Get interval to check splits at.
		{
			float scale{ (bounds_max - bounds_min) / splits };

			for (int i = 0; i < splits - 1; ++i)
			{
				float plane_cost{ (left_count[i] * left_area[i]) + (right_count[i] * right_area[i]) };
				if (plane_cost < best_cost)
				{
					best_axis = axis;
					best_position = bounds_min + scale * (i + 1);
					best_cost = plane_cost;
				}
			}
		}
	}

	return best_cost;
}