#include "precomp.h"
#include "cube_instance_bvh.h"


CubeInstanceBVH::CubeInstanceBVH() {}


CubeInstanceBVH::~CubeInstanceBVH()
{
	delete[] _nodes;
	delete[] _item_indicies;
}


// INTERSECT ITEM METHODS //

void CubeInstanceBVH::intersectItemForNearest(Ray& transformed_ray, const uint) const
{
	_cube->findNearest(transformed_ray);
}


bool CubeInstanceBVH::intersectItemForOcclusion(Ray& transformed_ray, TintData& tint_data, const uint) const
{
	return _cube->findOcclusion(transformed_ray, tint_data);
}


void CubeInstanceBVH::interesectItemForMaterialExit(Ray& original_ray, const uint material_type) const
{
	// Create a transformed ray to send into the BVH.
	Ray transformed_ray{ getTransformedRay(original_ray) };

	_cube->findMaterialExit(transformed_ray, material_type);

	// Move hit information into the original ray.
	transferDataToRay(transformed_ray, original_ray);
}


void CubeInstanceBVH::intersectItemForNearestToPlayer(Ray& ray, const uint, const int source_id) const
{
	// Player models do not interact with rays they create.
	if (source_id == _id)
	{
		return;
	}
		
	_cube->findNearest(ray);
}


void CubeInstanceBVH::setTransform(float3& scale, float3& rotation, float3& translation)
{
	// Set new values.
	_scale = scale;
	_rotate = rotation;
	_translate = translation;

	// Translate to pivot point.
	float3 to_pivot_translation{ -(make_float3(_cube->_size) * VOXELSIZE * 0.5f) };

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


// BUILD METHODS //

void CubeInstanceBVH::build()
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
	setBounds();
}


void CubeInstanceBVH::updateNodeBounds(uint) const
{
	// BVH has same bounds as the only child / item.
	_nodes[0]._bounds[0] = _cube->_bounds[0];
	_nodes[0]._bounds[1] = _cube->_bounds[1];
}


void CubeInstanceBVH::subdivide(const uint)
{
	// Only has 1 child / item.
}


float CubeInstanceBVH::findBestSplitPlane(BVHNode&, int&, float&) const
{
	// Only has 1 child / item.
	return 0.0f;
}


// CUBE API //

void CubeInstanceBVH::setID(const int id)
{
	_id = id;
	_cube->_id = id;
}


void CubeInstanceBVH::setCube(Cube* cube_instance)
{
	_cube = cube_instance;
}