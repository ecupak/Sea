#pragma once


class Stone : public Model
{
public:
	enum class Type
	{
		CHARGE,
		EMPTY,
		UPGRADE,
		NONE,
	};


	Stone() = default;
	
	void initialize(int id, Type _stone_type, uint color, uint non_metal_id, uint special_id, float up, Model& land);

	void addBeingLitObserver(Observer* observer);
	void addAudioEventObserver(Observer* observer);

	void update(const float delta_time);

	void activate();


	// Can be observed for these events.
	Subject _on_being_lit{ };
	Subject _on_audio_event{ };

	// Parent island surface.
	float _surface{ 0.0f };
	int _parent_island_id{ -1 };

	// General data.
	float _up{ 1.0f };
	Type _type{ Type::NONE };
	
	uint _color{ 0 };
	uint _non_metal_id{ 0 };
	
	union special_id
	{
		uint _emissive_id{ 0 };
		uint _mirror_id;
	} _special_material_id;

	// Active (hovering) state.
	bool _is_active{ false };
	bool _is_vanishing{ false };

	float3 _hover_offset{ 0.0f };
	float _hover_offset_range{ VOXELSIZE * 0.15f };
	float _hover_offset_speed{ VOXELSIZE * 0.15f };
	float _hover_offset_delta_sign{ 1.0f };
	
	float3 _hover_rotation{ 0.0f };
	float _hover_rotation_speed{ 0.1f };

	// Energized state.
	bool _is_energized{ false };
	float _max_energized_duration{ 1.0f };
	float _inverse_max_energized_duration{ 1.0f / _max_energized_duration };
	float _elapsed_energized_duration{ 0.0f };
	bool _was_light_delivered{ false };

private:
	void createBVH(Model& land);
	float getYOffset();
};

