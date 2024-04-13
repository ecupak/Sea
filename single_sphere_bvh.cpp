#include "precomp.h"
#include "single_sphere_bvh.h"


SingleSphereBVH::SingleSphereBVH(float3 position, float radius)
	: _sphere{ position, radius }
{
	_id = -2;
}


SingleSphereBVH::~SingleSphereBVH()
{
	delete[] _nodes;
	delete[] _item_indicies;
}


// INTERSECT ITEM METHODS //

void SingleSphereBVH::intersectItemForNearest(Ray& transformed_ray, const uint) const
{
	// Calculate discriminant.
	const float3 sphere_to_ray_origin{ transformed_ray.O - _sphere._position };
	const float a{ dot(transformed_ray.D, transformed_ray.D) };
	const float half_b{ dot(sphere_to_ray_origin, transformed_ray.D) };
	const float c{ dot(sphere_to_ray_origin, sphere_to_ray_origin) - _sphere._radius * _sphere._radius };

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
		transformed_ray.normal = (transformed_ray.IntersectionPoint() - _sphere._position) * _sphere._inverse_radius;
		transformed_ray._hit_data = _sphere._data;
		transformed_ray._id = _id;
	}
}


bool SingleSphereBVH::intersectItemForOcclusion(Ray& transformed_ray, TintData& tint_data, const uint) const
{
	// Calculate discriminant.
	const float3 sphere_to_ray_origin{ transformed_ray.O - _sphere._position };
	const float a{ dot(transformed_ray.D, transformed_ray.D) };
	const float half_b{ dot(sphere_to_ray_origin, transformed_ray.D) };
	const float c{ dot(sphere_to_ray_origin, sphere_to_ray_origin) - _sphere._radius * _sphere._radius };

	const float discriminant{ half_b * half_b - a * c };

	// If more than 0, there was an intersection.
	if (discriminant >= 1e-4f)
	{
		// Glass does not occlude.
		if (const bool is_cell_glass{ MaterialList::GetType(_sphere._data) == MaterialType::GLASS };
			is_cell_glass)
		{
			// We'll ignore the complexity of going through different glass colors.
			// Only the first glass hit (the closest to the surface sending the shadow ray) will be used.
			if (tint_data._voxel == 0)
			{
				// This is an approximation of Beer's law. I don't want to spend a lot of computing time here.
				// We'll assume we travel, on average, a distance equal to radius (half the sphere's thickness).
				tint_data._distance += _sphere._radius;
				tint_data._voxel = _sphere._data;
			}

			return false;
		}

		return true;
	}

	return false;
}


void SingleSphereBVH::intersectItemForNearestToPlayer(Ray&, const uint, const int) const
{
	return; // Spheres are not an object that will hinder the player's movement.
}


// BUILD METHODS //

void SingleSphereBVH::build()
{
	Timer t;

	delete[] _nodes;
	delete[] _item_indicies;
	_nodes_used = 1;

	// Set primitive count and create all nodes that will be used.
	const int N{ 1 };
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
	//subdivide(0);
	setBounds();
}


void SingleSphereBVH::updateNodeBounds(uint node_index) const
{
	float3 extent{ _sphere._radius };

	_nodes[node_index]._aabb_min = _sphere._position - extent;
	_nodes[node_index]._aabb_max = _sphere._position + extent;
}


void SingleSphereBVH::subdivide(const uint)
{
	// Will not be used - only 1 child.
}


float SingleSphereBVH::findBestSplitPlane(BVHNode&, int&, float&) const
{
	return 0.0f; // Will not be used - only 1 child.
}