#pragma once

// ALL TLAS/BLAS CODE HEAVILY INSPIRED (OR OUTRIGHT COPIED) FROM JACCO'S SERIES ON BVH CREATION
// [Credit] https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/


class SingleSphereBVH : public BVH
{
public:
	SingleSphereBVH(float3 position, float radius);


	~SingleSphereBVH() override;


	// INTERSECT ITEM METHODS //

	void intersectItemForNearest(Ray& transformed_ray, const uint item_index) const override;


	bool intersectItemForOcclusion(Ray& transformed_ray, TintData& tint_data, const uint item_index) const override;


	void intersectItemForNearestToPlayer(Ray& ray, const uint, const int) const override;


	// BUILD METHODS //

	void build() override;


	void updateNodeBounds(uint node_index) const override;


	void subdivide(const uint node_index);


	float findBestSplitPlane(BVHNode& node, int& best_axis, float& best_position) const;


	// Properties.
	Sphere _sphere;
};