#include "precomp.h"
#include "sphere_bvh.h"


SphereBVH::SphereBVH(std::vector<Sphere>& spheres)
	: _spheres{ spheres }
{	
	_id = -2;
}


SphereBVH::~SphereBVH()
{
	delete[] _nodes;
	delete[] _item_indicies;
}


// INTERSECT ITEM METHODS //

void SphereBVH::intersectItemForNearest(Ray& transformed_ray, const uint item_index) const
{
	const Sphere& sphere{ _spheres[item_index] };

	// Calculate discriminant.
	const float3 sphere_to_ray_origin{ transformed_ray.O - sphere._position };
	const float a{ dot(transformed_ray.D, transformed_ray.D) };
	const float half_b{ dot(sphere_to_ray_origin, transformed_ray.D) };
	const float c{ dot(sphere_to_ray_origin, sphere_to_ray_origin) - sphere._radius * sphere._radius };

	const float discriminant{ half_b * half_b - a * c };

	// Miss.
	if (discriminant < 0)
	{
		return;
	}

	// Precalculate values.
	const float inverse_a{ 1.0f / a };
	const float sqrt_discriminant{ sqrt(discriminant) };

	// Find which root of the sphere was hit.
	float root{ (-half_b - sqrt_discriminant) * inverse_a };
	if (root <= 0 || root >= Ray::t_max)
	{
		root = (-half_b + sqrt_discriminant) * inverse_a;
		if (root <= 0 || root >= Ray::t_max)
		{
			return;
		}
	}

	// If closer than ray's current t, set as hit.
	if (root < transformed_ray.t)
	{
		transformed_ray.t = root;
		transformed_ray.normal = (transformed_ray.IntersectionPoint() - sphere._position) * sphere._inverse_radius;
		transformed_ray._hit_data = sphere._data;
		transformed_ray._id = _id;
	}
}


bool SphereBVH::intersectItemForOcclusion(Ray& transformed_ray, TintData& tint_data, const uint item_index) const
{
	const Sphere& sphere{ _spheres[item_index] };

	// Glass does not occlude.
	if (const bool is_cell_glass{ MaterialList::GetType(sphere._data) == MaterialType::GLASS };
		is_cell_glass)
	{
		// We'll ignore the complexity of going through different glass colors.
		// Only the first glass hit (the closest to the surface sending the shadow ray) will be used.
		if (tint_data._voxel == 0)
		{
			// This is an approximation of Beer's law. I don't want to spend a lot of computing time here.
			// We'll assume we travel, on average, a distance equal to radius (half the sphere's thickness).
			tint_data._distance += sphere._radius;
			tint_data._voxel = sphere._data;
		}

		return false;
	}

	// Calculate discriminant.
	const float3 sphere_to_ray_origin{ transformed_ray.O - sphere._position };
	const float a{ dot(transformed_ray.D, transformed_ray.D) };
	const float half_b{ dot(sphere_to_ray_origin, transformed_ray.D) };
	const float c{ dot(sphere_to_ray_origin, sphere_to_ray_origin) - sphere._radius * sphere._radius };

	const float discriminant{ half_b * half_b - a * c };

	// If more than 0, there was an intersection.
	return discriminant >= 1e-4f;
}


void SphereBVH::intersectItemForNearestToPlayer(Ray&, const uint, const int) const
{
	return; // Spheres are not an object that will hinder the player's movement.
}


// BUILD METHODS //

void SphereBVH::build()
{
	Timer t;

	delete[] _nodes;
	delete[] _item_indicies;
	_nodes_used = 1;

	// Set primitive count and create all nodes that will be used.
	const int N{ static_cast<int>(_spheres.size()) };
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


void SphereBVH::updateNodeBounds(uint node_index) const
{
	BVHNode& node{ _nodes[node_index] };
		
	node._aabb_min = float3{ 1e30f };
	node._aabb_max = float3{ -1e30f };

	// Check all primitives to get the smallest and largest point.
	for (int i = 0; i < node._child_count; ++i)
	{
		const uint sphere_index{ _item_indicies[node._significant_index + i] };
		Sphere& sphere{ _spheres[sphere_index] };

		float3 extent{ sphere._radius };

		node._aabb_min = fminf(node._aabb_min, sphere._position - extent);
		node._aabb_max = fmaxf(node._aabb_max, sphere._position + extent);
	}
}


void SphereBVH::subdivide(const uint node_index)
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
		if (_spheres[left_i]._position[axis] < split_position)
		{
			++left_i;
		}
		else
		{
			std::swap(_spheres[left_i], _spheres[right_i--]);
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


float SphereBVH::findBestSplitPlane(BVHNode& node, int& best_axis, float& best_position) const
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
				Sphere& sphere{ _spheres[_item_indicies[node._significant_index + i]] };

				bounds_min = min(bounds_min, sphere._position[axis]);
				bounds_max = max(bounds_max, sphere._position[axis]);
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
				Sphere& sphere{ _spheres[_item_indicies[node._significant_index + i]] };

				// Easier to make a sphere aabb for the grow calls below.
				float3 extent{ sphere._radius };
				aabb sphere_box{ sphere._position - extent, sphere._position + extent };

				int bin_index{ min(splits - 1, static_cast<int>((sphere._position[axis] - bounds_min) * scale)) };
				bins[bin_index]._count++;
				bins[bin_index]._bounds.Grow(sphere_box);
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