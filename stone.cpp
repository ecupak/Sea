#include "precomp.h"
#include "stone.h"


void Stone::initialize(int id, Type stone_type, uint color, uint non_metal_id, uint special_id, float up, Model& land)
{
	// Set data.
	_parent_island_id = id;
	_up = up;
	_type = stone_type;
	_color = color;
	_non_metal_id = non_metal_id;
	_surface = land._position.y + land._offset.y * _up;
	
	if (_type == Type::UPGRADE)
	{
		_special_material_id._mirror_id = special_id;
	}
	else
	{
		_special_material_id._emissive_id = special_id;
	}

	// Charge and Upgrade stones start active.
	_is_active = (_type != Type::EMPTY);

	// Make BVH.
	createBVH(land);
}


void Stone::createBVH(Model& land)
{
	// Size.
	uint3 size{ 1 };

	// Create cube.
	_bvh.setCube(size);

	// Create voxel.
	switch (_type)
	{
		case Type::UPGRADE:
		{
			_bvh._cube.set(0, 0, 0, _special_material_id._mirror_id, _color);
			break;
		}
		case Type::CHARGE:
		{
			_bvh._cube.set(0, 0, 0, _non_metal_id, _color);
			break;
		}
		case Type::EMPTY:
		default:
		{
			_bvh._cube.set(0, 0, 0, _non_metal_id, 0xA2A2A2); // Grey stone = empty.
			break;
		}
	}

	// Finalize.
	_bvh.build();

	// Place.
	float random_angle{ 2.0f * PI * RandomFloat() };
	
	_offset = _bvh._bounds.bmax3 * 0.5f;
	_position = float3{
		land._position.x + (land._offset.x - VOXELSIZE * 2.0f) * cosf(random_angle),
		_surface + getYOffset() * _up,
		land._position.z + (land._offset.z - VOXELSIZE * 2.0f) * sinf(random_angle)
	};

	float3 translation{ _position - _offset };
	_bvh.setTransform(_bvh._scale, _bvh._rotate, translation);
}


void Stone::addBeingLitObserver(Observer* observer)
{
	_on_being_lit.addObserver(observer);
}


void Stone::addAudioEventObserver(Observer* observer)
{
	_on_audio_event.addObserver(observer);
}


void Stone::update(const float delta_time)
{
	if (_is_active)
	{
		// Move stone up and down.
		_hover_offset.y += delta_time * _hover_offset_speed * _hover_offset_delta_sign;

		if (fabsf(_hover_offset.y) > _hover_offset_range)
		{
			_hover_offset_delta_sign *= -1.f;
		}

		// Rotate stone.
		_hover_rotation.y += delta_time * _hover_rotation_speed;

		if (_is_energized)	// Rotate faster while energized.
		{
			_elapsed_energized_duration += delta_time;

			// Energized duration has expired. Remove glow.
			if (_elapsed_energized_duration > _max_energized_duration)
			{
				_is_energized = false;

				_bvh._cube.set(0, 0, 0, _non_metal_id, _color);
			}

			float speed_boost { 140.0f - lerp(0.0f, 140.0f, easeOut(_elapsed_energized_duration * _inverse_max_energized_duration)) };

			_hover_rotation.y += delta_time * _hover_rotation_speed * speed_boost;
		}

		// Transform.
		float3 translation { _position + _hover_offset - _offset };
		_bvh.setTransform(_bvh._scale, _hover_rotation, translation);
	}

	if (_is_vanishing)
	{		
		static float t{ 0.0f };

		if (t >= 1.0f)
		{
			_is_vanishing = false;
			return;
		}

		float3 scale{ lerp(1.0f, 0.0001f, t) };
		float3 rotate{ _bvh._rotate + float3{ 0.1f, 0.0f, 0.1f } };
		float3 translate{ _bvh._translate + float3{0.0f, VOXELSIZE * 0.05f * _up, 0.0f } };

		_bvh.setTransform(scale, rotate, translate);

		t += delta_time * 1.0f;
	}
}


void Stone::activate()
{
	switch (_type)
	{
		case Type::EMPTY:
		{
			_is_active = true;
			_type = Type::CHARGE;
			_position.y = _surface + getYOffset() * _up;
			_bvh._cube.set(0, 0, 0, _non_metal_id, _color);

			_on_being_lit.notify(EventType::ISLAND, _parent_island_id);
			_on_audio_event.notify(EventType::SFX, SFXEvent::ON_ISLAND_BRIGHTENED);

			break;
		}
		case Type::CHARGE:
		{
			_is_energized = true;
			_elapsed_energized_duration = 0.0f;
			_bvh._cube.set(0, 0, 0, _special_material_id._emissive_id, _color);			

			_on_audio_event.notify(EventType::SFX, SFXEvent::ON_CHARGE_STONE_ACTIVATION);

			break;
		}
		case Type::UPGRADE:
		{
			if (_is_active)
			{
				_is_active = false;
				_is_vanishing = true;
				_bvh._cube.set(0, 0, 0, _non_metal_id, 0xA2A2A2);
				_position.y = _surface + (VOXELSIZE * 0.5f);

				float3 translation{ _position - _offset };
				_bvh.setTransform(_bvh._scale, _bvh._rotate, translation);
								
				_on_being_lit.notify(EventType::ISLAND, _parent_island_id);
				_on_audio_event.notify(EventType::SFX, SFXEvent::ON_UPGRADE_STONE_ACTIVATION);
			}

			break;
		}
		default:
			break;
	}
}


float Stone::getYOffset()
{
	return VOXELSIZE * (_type == Type::EMPTY ? 0.5f : 0.75f);
}