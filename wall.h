#pragma once


class Wall : public Observer, public Model
{
public:
	enum class Type : unsigned int
	{
		GATE,
		BOUNDARY,
	};

	struct Data
	{
		Data(Type type, uint3 size, float y_rotation, float3 position, int island_id_to_observe, int gate_opening_index)
			: _size{ size }
			, _position{ position }
			, _type{ type }
			, _y_rotation{ y_rotation }
			, _island_id_to_observe{ island_id_to_observe }
			, _gate_opening_index{ gate_opening_index }
		{	}

		void append(uint wall_color, uint stripe_color, uint non_metal_id, uint emissive_id)
		{
			_wall_color = wall_color;
			_stripe_color = stripe_color;
			_non_metal_id = non_metal_id;
			_emissive_id = emissive_id;
		}

		uint3 _size{ 1 };					// 12
		float3 _position{ 1.0f };			// 12
		Type _type{ Type::BOUNDARY };		// 4
		float _y_rotation{ 0.0f };			// 4
		int _island_id_to_observe{ -1 };	// 4
		int _gate_opening_index{ -1 };		// 4

		uint _wall_color{ 0 };				// 4
		uint _stripe_color{ 0 };			// 4
		uint _non_metal_id{ 0 };			// 4
		uint _emissive_id{ 0 };				// 4
	};

	void initialize(Data& data);
	void initialize(Data& data, int2 gate_opening, uint glass_id, const char* design_filename);

	void onNotify(uint event_type, uint event_id) override;

private:
	void createWallBVH(Data& data);
	void openGate();

	int2 _gate_opening{ 0 };
	Type _type{ Type::BOUNDARY };
	int _dummy{ 0 };
};

