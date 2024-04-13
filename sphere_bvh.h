#pragma once

// ALL TLAS/BLAS CODE HEAVILY INSPIRED (OR OUTRIGHT COPIED) FROM JACCO'S SERIES ON BVH CREATION
// [Credit] https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/


class SphereBVH : public BVH
{
public:
	SphereBVH(std::vector<Sphere>& spheres);


	~SphereBVH() override;


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
	std::vector<Sphere>& _spheres;
};