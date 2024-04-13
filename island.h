#pragma once


/*
	it is far far away in the sea
	like all islands should be
*/


class Island : public Observer, public Model
{
public:
	enum class Location
	{
		ABOVE,
		BELOW,
		RISE,
		SINK,
		UNKNOWN,
	};


	struct Data
	{
		Data(Location location, uint island_color, float2 position, int2 facing, Stone::Type stone_type, uint stone_color, int upgrade_id, int2 island_ids_to_observe)
			: _location{ location }
			, _position{ position }
			, _facing{ facing }
			, _island_color{ island_color }
			, _stone_type{ stone_type }
			, _stone_color{ stone_color }
			, _island_ids_to_observe{ island_ids_to_observe }
			, _upgrade_id{ upgrade_id }
		{	}

		float2 _position{ 1.0f };
		int2 _facing{ 0 };
		int2 _island_ids_to_observe{ -1 };
		Location _location{ Location::UNKNOWN };
		Stone::Type _stone_type{ Stone::Type::NONE };
		uint _island_color{ 0 };
		uint _stone_color{ 0 };
		int _upgrade_id{ -1 };
	};


	Island() = default;

	void initialize(int id, Data& data, uint non_metal_id, uint glass_id);

	void update(const float delta_time);

	void addAudioEventObserver(Observer* observer);
	
	void onNotify(uint event_type, uint event_id) override;
		

	// Can be observed for these events.
	Subject _on_audio_event{};

	// The main island body.
	uint _color{ 0 };
	Location _location{ Location::UNKNOWN };

	// The pier for embarking/disembarking.
	Model _pier{ };
	
	// The island's stone.
	Stone _stone{ };


private:
	void createLandBVH(Data& data, uint non_metal_id);
	void createPierBVH(Data& data, uint non_metal_id);

	int2 _observed_ids{ -1 };
	bool _is_moving{ false };
	int _id{ -1 };
};