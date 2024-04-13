#pragma once


namespace Tmpl8 {
	class Scene;
}


class Player
{
public:
	struct Model
	{
		float3 _offset{ 0.0f };
		float3 _position{ 1.0f };
		float3 _look_at{ 1.0f };
		CubeBVH _bvh;
	};

	enum class World : unsigned int
	{
		DAY,
		NIGHT,
	};

	enum class TransportMode : unsigned int
	{
		SEA,
		LAND,
	};


	// Ctor.
	Player(Observer* audio_event_observer, Camera& camera, const float waterline);

	void initialize();

	void update(const float delta_time, const KeyboardManager& km, Scene* scene);

	void setTransportMode(const TransportMode transport_mode);

	Orb _orb{};

	Model _ship{};
	Model _avatar{};

	Subject _on_audio_event{};	
		
	// Player holds the world orientation.
	// Not really the world, but the local alterations to it
	// that both the player and camera will use.
	float4 _initial_world_ahead{ 0.0f };
	float4 _initial_world_up{ 0.0f };
	float4 _initial_world_right{ 0.0f };

	float3 _world_ahead{ 0.0f, 0.0f, 1.0f };
	float3 _world_up{ 0.0f, 1.0f, 0.0f };
	float3 _world_right{ 1.0f, 0.0f, 0.0f };
	
	float3 _ahead{ 1.0f, 0.0f, 0.0f };
	float3 _right{ 1.0f, 0.0f, 0.0f };
	float3 _up{ _world_up };

	// For shifting worlds.
	float3 _previous_world_up{ _world_up };
	float3 _last_good_position{ 1.0f };
	float3 _midpoints[2]{ 0.0f };

	// Camera connection.
	float3 _camera_percentages{ 1.0f };
	Camera& _camera;

	Model* _player{ nullptr };
	Stone* _nearby_stone{ nullptr };

	World _world{ World::DAY };
	TransportMode _transport_mode{ TransportMode::SEA };

	float _world_rotation{ 0.0f };

	float _waterline{ 0.0f };
	bool _is_using_telescope{ false };
	
	static constexpr float _min_velocity_threshold{ 0.001f };

	// Movement.
	float _forward_velocity{ 0.0f };
	float _sideways_velocity{ 0.0f };

	float _sea_friction{ 0.008f };
	float _land_friction{ 0.04f };
	float _air_friction{ 0.04f };

	// Ship / sea speeds.
	float _sea_forward_max_velocity{ 0.005f };
	float _sea_forward_acceleration{ 0.005f };

	float _sea_turning_max_velocity{ 0.02f };
	float _sea_turning_acceleration{ 0.2f };

	// Foot / land speeds.
	float _land_forward_max_velocity{ 0.004f };
	float _land_forward_acceleration{ 0.01f };
	
	bool _is_jumping{ false };
	float _gravity{ 0.014f };
	float _jump_velocity{ 0.0f };
	float _jump_impulse{ 0.005f };
	float _jump_max_velocity{ _jump_impulse };

	// Swap transport modes.
	bool _is_on_pier{ false };
	
	// Track when player lands on island/pier.
	bool _is_on_land{ false };

	// For animating events.
	float _t{ 0.0f };

	// Tracks when midway point of flip happens.
	bool _hit_midpoint{ false };

	bool _is_shifting_worlds{ false };
	bool _is_shrinking{ false };
	bool _is_leaping{ false };

	static constexpr float _PHYSICS_RAY_EPSILON{ 0.000001f };

private:
	void findNearestStone(Scene* scene);
	bool toggleTelescope(const KeyboardManager& km);

	// Ship / Sea controls.
	void handleShipControls(const float delta_time, const KeyboardManager& km, Scene* scene);
	bool swapToLand(const KeyboardManager& km);
	void moveShip(const float delta_time, const KeyboardManager& km);
	void handleShipMovement(const float delta_time, const KeyboardManager& km);
	void handleShipTurning(const float delta_time, const KeyboardManager& km);
	void handleShipCollisions(Scene* scene);
	void applyShipOrientationAndTransform();
	void calculateShipOrientation();
	bool flipBoat(const float delta_time);
	bool rotateWorlds(const float delta_time, const KeyboardManager& km);

	// Avatar / Land controls.
	void handleAvatarControls(const float delta_time, const KeyboardManager& km, Scene* scene);
	bool swapToSea(const KeyboardManager& km);
	void moveAvatar(const float delta_time, const KeyboardManager& km);
	void handleAvatarMovement(const float delta_time, const KeyboardManager& km);
	void handleAvatarJump(const float delta_time, const KeyboardManager& km);
	void handleAvatarCollisions(Scene* scene);
	void applyAvatarOrientationAndTransform(const KeyboardManager& km);
	bool shrinkingAvatar(const float delta_time);
	bool animatingRespawn(const float delta_time);
	bool leapingAvatar(const float delta_time);
	bool animatingLeap(const float delta_time);

	// Stone events.
	void checkNearbyStone(Scene* scene);

	// Orb events.
	void updateOrb(const float delta_time);
};

