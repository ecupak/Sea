#include "precomp.h"
#include "player.h"


Player::Player(Observer* audio_event_observer, Camera& camera, const float waterline)
	: _camera{ camera }
	, _waterline{ waterline }
{	
	_camera.setWorld(_world_ahead, _world_up, _world_right);
	_on_audio_event.addObserver(audio_event_observer);
}


void Player::initialize()
{
	// Get offsets for transformations (rotate around center, not corner).
	_ship._offset = _ship._bvh._bounds.bmax3 * 0.5f;
	_avatar._offset = _avatar._bvh._bounds.bmax3 * 0.5f;

	// Set ship's starting position and facing.
	_ship._position = float3{ 3.55f, _waterline, 1.80f };
	_ship._look_at = _ship._position + float3{ 0.0f, 0.0f, -1.0f };

	float3 translation{ _ship._position - _ship._offset };
	_ship._bvh.setTransform(_ship._bvh._scale, _ship._bvh._rotate, translation);

	// Set starting transport mode. Pretend player is coming from land.
	_is_on_land = true;
	setTransportMode(TransportMode::SEA);
}


void Player::setTransportMode(const TransportMode transport_mode)
{
	_transport_mode = transport_mode;

	// Exit previous mode, initialize new mode.
	// TODO: FSM for ship and avatar states?
	switch (_transport_mode)
	{
		// SHIP
		case TransportMode::SEA:
		{
			// Halt previous movement.
			_forward_velocity = 0.0f;
			_sideways_velocity = 0.0f;
			_jump_velocity = 0.0f;
			_is_jumping = false;

			// Change music if coming from land (did not jump off ship and land back on it).
			if (_is_on_land)
			{
				switch (_world)
				{
					case World::DAY:
					{
						_on_audio_event.notify(EventType::MUSIC, MusicEvent::ON_DAY_OCEAN);
						break;
					}
					case World::NIGHT:
					{
						_on_audio_event.notify(EventType::MUSIC, MusicEvent::ON_NIGHT_OCEAN);
						break;
					}
					default:
						break;
				}

				_is_on_land = false;
			}

			// Place avatar on ship.
			_avatar._position = _ship._position + float3{ 0.0f, VOXELSIZE, 0.0f };
			_last_good_position = _avatar._position;

			// Update orientation.
			applyShipOrientationAndTransform();

			// Set as current model.
			_player = &_ship;

			break;
		}

		// AVATAR
		case TransportMode::LAND:
		{						
			// Surprisingly, there isn't much to do when the avatar jumps off the boat.

			// Halt previous movement (maintain forward momentum).
			_sideways_velocity = 0.0f;

			// Last place on boat was safe.
			_last_good_position = _avatar._position;

			// Set as current model.
			_player = &_avatar;

			break;
		}

		default:
			break;
	}
}


void Player::update(const float delta_time, const KeyboardManager& km, Scene* scene)
{
	// Handle input.
	switch (_transport_mode)
	{
		case TransportMode::SEA:
		{
			handleShipControls(delta_time, km, scene);
			break;
		}
		case TransportMode::LAND:
		{
			handleAvatarControls(delta_time, km, scene);			
			break;
		}
	}

	// While camera is following player, always focus on a spot just above head.
	if (!_is_using_telescope && !_is_shifting_worlds)
	{
		_camera.setLookAt(_avatar._position + (_up * VOXELSIZE));
	}
}


void Player::findNearestStone(Scene* scene)
{
	static float stone_reach_squared{ (2.0f * VOXELSIZE) * (2.0f * VOXELSIZE) };
	
	_nearby_stone = nullptr;

	for (Island& island : scene->_islands)
	{
		float3 player_to_stone{ island._stone._position - _player->_position };

		float distance_squared_from_player_to_stone{ dot(player_to_stone, player_to_stone) };

		if (distance_squared_from_player_to_stone <= stone_reach_squared)
		{
			_nearby_stone = &island._stone;

			// There will never be more than 1 stone at this distance to player.
			break;
		}
	}
}


bool Player::toggleTelescope(const KeyboardManager& km)
{
	if (km.isJustPressed(Action::Telescope))
	{
		_is_using_telescope = !_is_using_telescope;

		_camera.enableTelescope(_is_using_telescope);

		// Hide player and orb far away.
		if (_is_using_telescope)
		{
			_avatar._position.y -= _up.y * 5.0f;

			float3 translation{ _avatar._position - _avatar._offset };
			_avatar._bvh.setTransform(_avatar._bvh._scale, _avatar._bvh._rotate, translation);
			_orb.applyOrientationAndTransform(_avatar._position, _up);
		}

		// Restore player and orb positions.
		else
		{
			_avatar._position = _last_good_position;

			float3 translation{ _avatar._position - _avatar._offset };
			_avatar._bvh.setTransform(_avatar._bvh._scale, _avatar._bvh._rotate, translation);
			_orb.applyOrientationAndTransform(_avatar._position, _up);
		}
	}

	return _is_using_telescope;
}


// SHIP CONTROLS + MOVEMENT //

void Player::handleShipControls(const float delta_time, const KeyboardManager& km, Scene* scene)
{
	if (rotateWorlds(delta_time, km))	{ return; }
	if (toggleTelescope(km))			{ return; }
	if (swapToLand(km))					{ return; }

	updateOrb(delta_time);

	moveShip(delta_time, km);
	handleShipCollisions(scene);
	applyShipOrientationAndTransform();
}


bool Player::swapToLand(const KeyboardManager& km)
{
	// Swap transportation modes on player jump.
	handleAvatarJump(0.0f, km);

	if (_is_jumping)
	{
		_ahead = _camera._no_pitch_ahead;
		_right = _camera._right;

		setTransportMode(TransportMode::LAND);

		return true;
	}

	return false;
}


void Player::moveShip(const float delta_time, const KeyboardManager& km)
{
	handleShipMovement(delta_time, km);
	handleShipTurning(delta_time, km);

	// Update position and facing based on current velocities.
	_ship._position += _ahead * _forward_velocity;
	_ship._look_at = _ship._position + _ahead + (_right * _sideways_velocity);
}


void Player::handleShipMovement(const float delta_time, const KeyboardManager& km)
{
	// Gain velocity.
	if (km.isPressed(Action::Forward))
	{
		_forward_velocity += _sea_forward_acceleration * delta_time;
	}
	else
	{
		_forward_velocity -= _sea_friction * delta_time;
		if (_forward_velocity < _min_velocity_threshold)
		{
			_forward_velocity = 0.0f;
		}
	}

	// Apply limits.			
	_forward_velocity = fminf(_forward_velocity, _sea_forward_max_velocity);
}


void Player::handleShipTurning(const float delta_time, const KeyboardManager& km)
{
	// Gain velocity.
	if (float left_right_steering{ (km.isPressed(Action::Left) ? -1.0f : 0.0f) + (km.isPressed(Action::Right) ? 1.0f : 0.0f) };
		left_right_steering != 0.0f)
	{
		_sideways_velocity += left_right_steering * _sea_turning_acceleration * delta_time;
	}
	else
	{
		float sideways_sign{ _sideways_velocity > 0.0f ? 1.0f : _sideways_velocity < 0.0f ? -1.0f : 0.0f };
		_sideways_velocity -= 10.0f * _sea_friction * delta_time * sideways_sign;

		if (fabsf(_sideways_velocity) < 10.0f *_min_velocity_threshold)
		{
			_sideways_velocity = 0.0f;
		}
	}

	// Apply limits.
	_sideways_velocity = clamp(_sideways_velocity, -_sea_turning_max_velocity, _sea_turning_max_velocity);
}


void Player::handleShipCollisions(Scene* scene)
{
	float3 vertical_offset{ 0.001f * _up };

	// Check ahead.
	Ray player_ray{ _ship._position + vertical_offset, _ahead + vertical_offset, 0, _ship._offset.x + _PHYSICS_RAY_EPSILON, false };
	scene->findNearestToPlayer(player_ray, _ship._bvh._id);

	// If hit something ...
	if (player_ray._hit_data)
	{
		// Halt forward movement.
		_forward_velocity = 0.0f;

		// Extract from object.
		if (player_ray.t < _ship._offset.x)
		{
			float penetration{ _ship._offset.x - player_ray.t };

			_ship._position += -_ahead * penetration;
		}
	}
}


void Player::applyShipOrientationAndTransform()
{
	// Calculate updated ahead, right, and up vectors.
	calculateShipOrientation();

	// Apply transformations to the model.
	float3 rotation{ -_world_rotation, -atan2(_ahead.z, _ahead.x), 0.0f };
	float3 translation{ _ship._position - _ship._offset };

	_ship._bvh.setTransform(_ship._bvh._scale, rotation, translation);

	// For the ship, also move avatar so it "rides" the ship.
	_avatar._position = _ship._position + (_up * VOXELSIZE);
	translation = _avatar._position - _avatar._offset;

	_avatar._bvh.setTransform(_avatar._bvh._scale, rotation, translation);

	// When the avatar moves, move the orb to follow.
	_orb.applyOrientationAndTransform(_avatar._position, _up);
}


void Player::calculateShipOrientation()
{
	_ahead = normalize(_ship._look_at - _ship._position);
	_right = normalize(cross(_world_up, _ahead));
	_up = normalize(cross(_ahead, _right));
}


bool Player::flipBoat(const float delta_time)
{
	// Advance t.
	_t = fminf(_t + delta_time * 0.4f, 1.0f);
	
	// Change music at halfway point.
	if (!_hit_midpoint && _t >= 0.5f)
	{
		switch (_world)
		{
			case World::DAY:
			{
				_on_audio_event.notify(EventType::MUSIC, MusicEvent::ON_SWITCH_TO_NIGHT);
				_world = World::NIGHT;
				break;
			}
			case World::NIGHT:
			{
				_on_audio_event.notify(EventType::MUSIC, MusicEvent::ON_SWITCH_TO_DAY);
				_world = World::DAY;			
				break;
			}
		}

		_hit_midpoint = true;
	}

	// Use Rodrigues' rotation formula to create a rotation matrix.
	// [Credit] Led to investigate the formula by ChatGPT
	// Attempted to implement my own using Wolfram MathWorld:
	// https://mathworld.wolfram.com/RodriguesRotationFormula.html
	// But it did not work. Ultimately used Jacco's built-in Rotate().
	// Left here for prosperity:
	/*
	mat4 I{ mat4::Identity() };
	float3& w{ _ahead };
	mat4 antisymmetric_matrix;
	{
		antisymmetric_matrix[0] = 0;
		antisymmetric_matrix[1] = -w.z;
		antisymmetric_matrix[2] = w.y;

		antisymmetric_matrix[4] = w.z;
		antisymmetric_matrix[5] = 0;
		antisymmetric_matrix[6] = -w.x;

		antisymmetric_matrix[8] = -w.y;
		antisymmetric_matrix[9] = w.x;
		antisymmetric_matrix[10] = 0;
	}
	mat4 rrf_matrix{ I 
		+ antisymmetric_matrix * sinf(_world_rotation) 
		+ antisymmetric_matrix * antisymmetric_matrix 
		* (1.0f - cosf(_world_rotation)) };
	*/

	// Rotate world vectors around arbitrary axis (player's ahead).
	_world_rotation = lerp(0, PI, _t);
	mat4 rotation_matrix{ mat4::Rotate(_ahead, _world_rotation) };

	_world_ahead = make_float3(normalize(rotation_matrix * _initial_world_ahead));
	_world_right = make_float3(normalize(rotation_matrix * _initial_world_right));
	_world_up	 = make_float3(normalize(rotation_matrix * _initial_world_up));

	// Apply to models.
	applyShipOrientationAndTransform();	

	// Update camera.
	_camera.setWorld(_world_ahead, _world_up, _world_right);	
	_camera.setLookAt(_avatar._position + (_up * VOXELSIZE));


	return (_t < 1.0f);
}


bool Player::rotateWorlds(const float delta_time, const KeyboardManager& km)
{
	// Start shifting worlds.
	if (!_is_shifting_worlds && km.isJustPressed(Action::Shift_Worlds))
	{
		// Halt movement.
		_forward_velocity = 0.0f;
		_sideways_velocity = 0.0f;

		// Reset progress tracker.
		_t = 0.0f;
		_hit_midpoint = false;

		// Store current as previous.
		_previous_world_up = _world_up;
		
		// Store state of current.
		_initial_world_ahead = make_float4(_world_ahead, 0.0f);
		_initial_world_right = make_float4(_world_right, 0.0f);
		_initial_world_up	 = make_float4(_world_up,	 0.0f);

		// Play sfx.
		_on_audio_event.notify(EventType::SFX, SFXEvent::ON_SHIFT_WORLDS);

		_is_shifting_worlds = true;
	}

	// Already shifting - do the shift.
	else if (_is_shifting_worlds)
	{
		_is_shifting_worlds = flipBoat(delta_time);
		_camera._is_shifting_worlds = _is_shifting_worlds;
	}

	return _is_shifting_worlds;
}


// AVATAR CONTROLS + MOVEMENT //

void Player::handleAvatarControls(const float delta_time, const KeyboardManager& km, Scene* scene)
{
	if (animatingRespawn(delta_time))	{ return; }
	if (animatingLeap(delta_time))		{ return; }
	if (toggleTelescope(km))			{ return; }
	if (swapToSea(km))					{ return; }
	
	moveAvatar(delta_time, km);
	handleAvatarCollisions(scene);
	applyAvatarOrientationAndTransform(km);
	
	checkNearbyStone(scene);
	updateOrb(delta_time);
}


// [Credit] Lynn (230137)
// Helping figure out the math for tracing a semi-circle in 3D along an arbitrary axis.
bool Player::swapToSea(const KeyboardManager& km)
{
	// If standing on pier, always allow swapping back to ship. If lost in the darkness, swap back too.
	bool is_warping_from_pier{ _is_on_pier && km.isJustPressed(Action::Board) };
	bool is_warping_from_the_dark{ _world == World::NIGHT && km.isJustPressed(Action::Board)
		&& _orb._intensity == 0.0f};

	if (is_warping_from_pier || is_warping_from_the_dark)
	{
		// Sound to go with animation.
		_on_audio_event.notify(EventType::SFX, SFXEvent::ON_PIER_EMBARK);

		// Find halfway point along ground (origin) of leap distance
		float3 half_parallel_vector{ 0.5f * (_avatar._position - (_ship._position + _up * float3{0.0f, VOXELSIZE, 0.0f })) };
		float3 origin{ _avatar._position - half_parallel_vector };

		// Find radius and get perpendicular half vector (y, going up).
		float radius{ length(half_parallel_vector) };
		float3 half_perpendicular_vector{ _up * float3{0.0f, radius, 0.0f} };
		
		// Find 2 midpoints on arc connecting avatar and ship.
		float pitch{ PI * 0.33f };
		_midpoints[0] = origin + (cosf(pitch)  * half_parallel_vector) + (sin(pitch)  * half_perpendicular_vector);
		
		pitch = PI * 0.66f;
		_midpoints[1] = origin + (cosf(pitch) * half_parallel_vector) + (sin(pitch) * half_perpendicular_vector);
		
		// Begin leap.
		_t = 0.0f;
		_is_leaping = true;
		
		return true;
	}

	return false;
}


void Player::moveAvatar(const float delta_time, const KeyboardManager& km)
{
	handleAvatarMovement(delta_time, km);
	handleAvatarJump(delta_time, km);

	// Update vectors while not jumping. When jumping/falling, continue to use last known vectors while on ground
	if (!_is_jumping)
	{
		_ahead = _camera._no_pitch_ahead;		
		_right = _camera._right;
	}

	// Update position based on current velocities.
	_avatar._position += _ahead * _forward_velocity + _right * _sideways_velocity + _world_up * _jump_velocity;
	_avatar._look_at = _avatar._position + _camera._no_pitch_ahead;	
}


void Player::handleAvatarMovement(const float delta_time, const KeyboardManager& km)
{
	// Gain velocity. QoL: Immediately remove velocity of the opposite direction (unless let go complete, then slide to stop).
	
	// Forward.
	if (km.isPressed(Action::Forward))
	{
		_forward_velocity = fmaxf(0.0f, _forward_velocity + _land_forward_acceleration * delta_time);
	}
	else if (km.isPressed(Action::Backward))
	{
		_forward_velocity = fminf(_forward_velocity - _land_forward_acceleration * delta_time, 0.0f);
	}
	else
	{
		float forward_sign{ _forward_velocity > 0.0f ? 1.0f : _forward_velocity < 0.0f ? -1.0f : 0.0f };
		_forward_velocity -= _land_friction * delta_time * forward_sign;
		
		if (fabsf(_forward_velocity) < _min_velocity_threshold)
		{
			_forward_velocity = 0.0f;
		}
	}

	if (km.isPressed(Action::Left))
	{
		_sideways_velocity = fminf(_sideways_velocity - _land_forward_acceleration * delta_time, 0.0f);
	}
	else if (km.isPressed(Action::Right))
	{
		_sideways_velocity = fmaxf(0.0f, _sideways_velocity + _land_forward_acceleration * delta_time);
	}
	else
	{
		float sideways_sign{ _sideways_velocity > 0.0f ? 1.0f : _sideways_velocity < 0.0f ? -1.0f : 0.0f };
		_sideways_velocity -= _land_friction * delta_time * sideways_sign;

		if (fabsf(_sideways_velocity) < _min_velocity_threshold)
		{
			_sideways_velocity = 0.0f;
		}
	}

	// Apply limits.			
	_forward_velocity = clamp(_forward_velocity, -_land_forward_max_velocity, _land_forward_max_velocity);
	_sideways_velocity = clamp(_sideways_velocity, -_land_forward_max_velocity, _land_forward_max_velocity);
}


void Player::handleAvatarJump(const float delta_time, const KeyboardManager& km)
{

	// If in air, fall.
	if (_is_jumping)
	{
		_jump_velocity -= _gravity * delta_time;
	}

	// If not jumping and wants to jump, jump!	
	else if (km.isPressed(Action::Jump))
	{
		_on_audio_event.notify(EventType::SFX, SFXEvent::ON_PLAYER_JUMP);

		_jump_velocity = _jump_impulse;

		_is_jumping = true;
	}	

	// Apply limits.			
	//_jump_velocity = clamp(_jump_velocity, -_jump_max_velocity, _jump_max_velocity);
}


void Player::handleAvatarCollisions(Scene* scene)
{
	// Avatar model is a cube - same distance to each side (except corners).
	static float minimum_ray_length{ 0.5f * VOXELSIZE };
	static float collision_ray_length{ minimum_ray_length + _PHYSICS_RAY_EPSILON };
	static float3 y_offset{ 0.0f, -0.25 * VOXELSIZE, 0.0f };

	// Check ahead.
	Ray player_ray{ _avatar._position + y_offset, _ahead, 0, collision_ray_length, false };
	scene->findNearestToPlayer(player_ray, _avatar._bvh._id);
	
	// If hit something ...
	if (player_ray._hit_data)
	{
		// Halt forward movement.
		_forward_velocity = 0.0f;
		
		// Extract from object.
		if (player_ray.t < minimum_ray_length)
		{
			float penetration{ minimum_ray_length - player_ray.t };

			_avatar._position += -_ahead * penetration;
		}
	}

	// Check behind.
	player_ray = Ray{ _avatar._position + y_offset, -_ahead, 0, collision_ray_length, false };
	scene->findNearestToPlayer(player_ray, _avatar._bvh._id);
	
	// If hit something ...
	if (player_ray._hit_data)
	{
		// Halt forward movement.
		_forward_velocity = 0.0f;

		// Extract from object.
		if (player_ray.t < minimum_ray_length)
		{
			float penetration{ minimum_ray_length - player_ray.t };

			_avatar._position += _ahead * penetration;
		}
	}

	// Check left.
	player_ray = Ray{ _avatar._position + y_offset, -_right, 0, collision_ray_length, false };
	scene->findNearestToPlayer(player_ray, _avatar._bvh._id);
	
	// If hit something ...
	if (player_ray._hit_data)
	{
		// Halt strafe movement.
		_sideways_velocity = 0.0f;

		// Extract from object.
		if (player_ray.t < minimum_ray_length)
		{
			float penetration{ minimum_ray_length - player_ray.t };

			_avatar._position += _right * penetration;
		}
	}

	// Check right.
	player_ray = Ray{ _avatar._position + y_offset, _right, 0, collision_ray_length, false };
	scene->findNearestToPlayer(player_ray, _avatar._bvh._id);

	// If hit something ...
	if (player_ray._hit_data)
	{
		// Halt stafe movement.
		_sideways_velocity = 0.0f;

		// Extract from object.
		if (player_ray.t < minimum_ray_length)
		{
			float penetration{ minimum_ray_length - player_ray.t };

			_avatar._position += -_right * penetration;
		}
	}

	// Check down.
	player_ray = Ray{ _avatar._position, -_up, 0, collision_ray_length, false };
	scene->findNearestToPlayer(player_ray, _avatar._bvh._id);

	// If hit something ...
	if (player_ray._hit_data)
	{		
		// Halt vertical movement.
		_jump_velocity = 0.0f;
		_is_jumping = false;

		// If fell into water, will warp back to safe spot (after shrink animation).
		if (MaterialList::GetType(player_ray._hit_data) == MaterialType::WATER)
		{
			_on_audio_event.notify(EventType::SFX, SFXEvent::ON_WATER_DEATH);
			
			_is_shrinking = true;
			_t = 0.0f;
			
			return;
		}

		// Board ship if landed on it.
		if (player_ray._id == _ship._bvh._id)
		{
			setTransportMode(TransportMode::SEA);
			return;
		}

		// Check if standing on pier.
		_is_on_pier = player_ray._id == -9;		

		// Extract from object.
		if (player_ray.t < minimum_ray_length)
		{
			float penetration{ minimum_ray_length - player_ray.t };

			_avatar._position += _up * penetration;
		}

		// Store as last known good position. Will warp back here if player falls into water.
		_last_good_position = _avatar._position;

		// If first time touching land (was on ship), update music.
		if (!_is_on_land)
		{
			switch (_world)
			{
				case World::DAY:
				{
					_on_audio_event.notify(EventType::MUSIC, MusicEvent::ON_DAY_ISLAND);
					break;
				}
				case World::NIGHT:
				{
					_on_audio_event.notify(EventType::MUSIC, MusicEvent::ON_NIGHT_ISLAND);
					break;
				}
				default:
					break;
			}

			_is_on_land = true;
		}

	}
	else if (!_is_jumping)
	{
		_is_jumping = true;
	}
}


void Player::applyAvatarOrientationAndTransform(const KeyboardManager&)
{
	// Uses camera's ahead, right, and up vectors - no orientation calculation needed.

	// Apply transformations to the model.
	float y_rot = atan2(_camera._ahead.x, _camera._ahead.z);	
	float3 rotation{ 0.0f, y_rot, 0.0f };

	float3 translation{ _avatar._position - _avatar._offset };

	_avatar._bvh.setTransform(_avatar._bvh._scale, rotation, translation);

	// When the avatar moves, move the orb to follow.
	_orb.applyOrientationAndTransform(_avatar._position, _up);
}


bool Player::shrinkingAvatar(const float delta_time)
{
	_t = fminf(_t + delta_time, 1.0f);

	float3 scale{ lerp(1.0f, 0.0001f, _t) };
	float3 rotate{ _avatar._bvh._rotate + float3{ 0.1f, 0.0f, 0.1f } };

	_avatar._bvh.setTransform(scale, rotate, _avatar._bvh._translate);

	return (_t < 1.0f);
}


bool Player::animatingRespawn(const float delta_time)
{
	if (_is_shrinking)
	{
		if (!shrinkingAvatar(delta_time))
		{
			_is_shrinking = false;

			// Restore shape.
			float3 restoredScale{ 1.0f };
			float3 restoredRotation{ 0.0f };
			float3 respawnLocation{ _last_good_position - _avatar._offset };
			_avatar._bvh.setTransform(restoredScale, restoredRotation, respawnLocation);

			// Place at prior safe spot.
			_avatar._position = _last_good_position;
			_orb.applyOrientationAndTransform(_avatar._position, _up);

			_forward_velocity = 0.0f;
			_sideways_velocity = 0.0f;
		}

		return true;
	}

	return false;
}


bool Player::leapingAvatar(const float delta_time)
{	
	_t = fminf(_t + delta_time, 1.0f);

	// Find new position along curve.
	float3 destination{ _ship._position + _up * float3{0.0f, VOXELSIZE, 0.0f } };
	_avatar._position = getBezierPoint(_avatar._position, _midpoints[0], _midpoints[1], destination, _t);
	
	// Get and apply transform.
	float3 rotate{ _avatar._bvh._rotate + float3{ 0.0f, 0.2f, 0.0f } };
	float3 translate{ _avatar._position - _avatar._offset };

	_avatar._bvh.setTransform(_avatar._bvh._scale, rotate, translate);
	_orb.applyOrientationAndTransform(_avatar._position, _up);

	return (_t < 1.0f);
}


bool Player::animatingLeap(const float delta_time)
{
	if (_is_leaping)
	{
		if (!leapingAvatar(delta_time))
		{
			_is_leaping = false;

			setTransportMode(TransportMode::SEA);
		}
		
		return true;
	}

	return false;
}


// STONE UPDATE //

void Player::checkNearbyStone(Scene* scene)
{
	// Check if near charging stone.
	findNearestStone(scene);
	if (!_nearby_stone) { return; }

	// Interact with stone.
	switch (_nearby_stone->_type)
	{
		case Stone::Type::UPGRADE:
		{
			_nearby_stone->activate();				
			break;
		}
		case Stone::Type::CHARGE:
		{
			// Play charging sound only if stone is not doing energized "dance".
			if (!_nearby_stone->_is_energized)
			{
				_nearby_stone->activate();				
			}

			if (_orb.charge(_nearby_stone->_color))
			{
				if (!_nearby_stone->_was_light_delivered)
				{
					_on_audio_event.notify(EventType::MUSIC, MusicEvent::ON_ORB_HAS_CHARGE);
				}
			}
			
			break;
		}
		case Stone::Type::EMPTY:
		{
			if (_orb._intensity > 0.0f || _orb._color == _nearby_stone->_color)
			{
				_nearby_stone->activate();
			}
			break;
		}
		default:
			break;
	}	
}


// ORB UPDATE //

void Player::updateOrb(const float delta_time)
{
	// If orb loses all charge, cue music.
	if (_orb.drain(delta_time))
	{
		_on_audio_event.notify(EventType::SFX, SFXEvent::ON_ORB_REACHED_EMPTY);
		_on_audio_event.notify(EventType::MUSIC, MusicEvent::ON_ORB_LOST_CHARGE);
	}

	// Build up charge if was within range.
	_orb.update(delta_time);
}