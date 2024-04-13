#pragma once


class CubeInstanceBVH : public BVH
{
public:
	CubeInstanceBVH();

	~CubeInstanceBVH() override;


	// INTERSECT ITEM METHODS //

	void intersectItemForNearest(Ray& transformed_ray, const uint) const override;


	bool intersectItemForOcclusion(Ray& transformed_ray, TintData& tint_data, const uint) const override;


	void interesectItemForMaterialExit(Ray& original_ray, const uint material_type) const override;


	void intersectItemForNearestToPlayer(Ray& ray, const uint, const int source_id) const override;


	void setTransform(float3& scale, float3& rotation, float3& translation) override;


	// BUILD METHODS //

	void build() override;


	void updateNodeBounds(uint node_index) const override;


	void subdivide(const uint node_index);


	float findBestSplitPlane(BVHNode& node, int& best_axis, float& best_position) const;


	// CUBE API //
	void setID(const int id) override;


	void setCube(Cube* cube_instance);


	// Properties.
	Cube* _cube{ nullptr };

	bool _is_player_cube{ false };
};