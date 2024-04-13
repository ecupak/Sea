#pragma once


class Orb
{
public:
	Orb() = default;

	void initialize(Material* material, uint material_data, Light* inner_light);
		
	void applyOrientationAndTransform(float3& avatar_position, float3& up);

	void update(const float delta_time);

	bool charge(uint charge_color);

	bool drain(const float delta_time);

	void applyIntensityToLight();


	float3 _offset{ 0.0f };
	float3 _position{ 1.0f };
	float3 _look_at{ 1.0f };
	
	Material* _material;
	uint _material_data{ 0 };
	uint _color{ 0 };

	bool _is_charging{ false };

	float _intensity{ 0.0f };
	float _max_intensity{ 10.0f };
	float inverse_max_intensity{ 1.0f / _max_intensity };
	float _charge_speed{ 2.0f };
	float _drain_speed{ 0.1f };

	SingleSphereBVH _bvh{ float3{0.0f}, 0.01f };

	Light* _inner_light{ nullptr };
	float _max_light_intensity{ 2.0f };


private:
	float3 convertUintToFloat3(uint color);
};

