#pragma once

// ALL TLAS/BLAS CODE HEAVILY INSPIRED (OR OUTRIGHT COPIED) FROM JACCO'S SERIES ON BVH CREATION
// [Credit] https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/


class CubeBVH : public BVH
{
public:
	CubeBVH();
	CubeBVH(const uint3& size);
	
	~CubeBVH() override;


	// INTERSECT ITEM METHODS //

	void intersectItemForNearest(Ray& transformed_ray, const uint) const override;


	bool intersectItemForOcclusion(Ray& transformed_ray, TintData& tint_data, const uint) const override;


	void interesectItemForMaterialExit(Ray& original_ray, const uint material_type) const override;


	void intersectItemForNearestToPlayer(Ray& ray, const uint, const int source_id) const override;


	void intersectItemForVoxelErasure(Ray& ray, const uint, Cube*& modified_cube) override;


	// BUILD METHODS //

	void build() override;


	void updateNodeBounds(uint node_index) const override;


	void subdivide(const uint node_index);


	float findBestSplitPlane(BVHNode& node, int& best_axis, float& best_position) const;

	
	// TRANSFORMS //

	void setTransform(const float3& scale, const float3& rotation, const float3& translation) override;


	// CUBE API //

	void setID(const int id) override;
	

	void setCube(const uint3& size);


	// Properties.
	Cube _cube;

	bool _is_player_cube{ false };
};