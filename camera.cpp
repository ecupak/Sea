#include "precomp.h"
#include "camera.h"


Camera::Camera()
	: _position{ float3(-5.0f, 0.5f, -1.0f) }
	, _look_at{ float3{0.5f, 0.5f, 0.5f} }
	, _aspect_ratio{ static_cast<float>(SCRWIDTH) / static_cast<float>(SCRHEIGHT) }
{
	setFOV(_fov);
	calculateOrientation();
}


Camera Camera::retire()
{
	float3 bottom_right{ _top_right - (_top_left - _bottom_left) };

	_top_normal = cross(_top_right - _position, _top_left - _position);
	_left_normal = cross(_top_left - _position, _bottom_left - _position);
	_right_normal = cross(bottom_right - _position, _top_right - _position);
	_bottom_normal = cross(_bottom_left - _position, bottom_right - _position);

	return *this;
}


void Camera::setMode(CamMode mode)
{
	_cam_mode = mode;

	switch (mode)
	{
	case CamMode::PINHOLE:
	{
		_speed = 1.3f;
		setFOV(_old_fov);
		calculateOrientation();
		break;
	}
	case CamMode::THIN_LENS:
	{
		_speed = 1.3f;
		setFOV(_old_fov);
		Ray::setEpsilon(7);
		calculateOrientation();
		setupApertureShape();
		break;
	}
	case CamMode::FISHEYE:
	{
		_speed = 0.3f;
		setFOV(180.0f);
		calculateOrientation();
		_z_length = length(0.5f * _v_vec);
		break;
	}
	default:
		break;
	}
}


const bool Camera::update(const float delta_time, const KeyboardManager& km, Scene* scene)
{
	// Pause updates while game does not have focus.
	if (!WindowHasFocus()) return false;

	// Toggle between debug and player cam.
	if (IsKeyDown(GLFW_KEY_LEFT_CONTROL))
	{
		_in_player_mode = false;
		_is_mouse_initialized = false;
	}
	if (IsKeyDown(GLFW_KEY_RIGHT_CONTROL))
	{
		_in_player_mode = true;
	}

	// Update focal distance to target distance, if different.
	if (_is_changing_focus)
	{
		_focal_plane_distance = lerp(_focal_plane_distance, _target_focal_plane_distance, _focal_change_step_percent);

		float focal_change_speed{ 2.0f };
		_focal_change_step_percent = clamp(_focal_change_step_percent + (focal_change_speed * delta_time), 0.0f, 1.0f);

		float threshold{ 0.0001f };
		_is_changing_focus = fabsf(_focal_plane_distance - _target_focal_plane_distance) > threshold;
	}

	
	// Update camera vectors.
	if (_in_player_mode)
	{
		// Get mouse position.
		if (!_is_mouse_initialized)
		{
			_previous_mouse_position = km.getMousePosition();

			if (_previous_mouse_position.x >= 0 && _previous_mouse_position.y >= 0)
			{
				_is_mouse_initialized = true;
			}
		}

		int2 mouse_delta{ km.getMousePosition() - _previous_mouse_position };
		_previous_mouse_position = km.getMousePosition();

		// Update camera based on current mode.
		switch (_cam_mode)
		{
			case CamMode::PINHOLE:
			{
				updatePlayerCamera(mouse_delta, scene);
				break;
			}
			case CamMode::FISHEYE:
			{
				updateTelescopeCamera(delta_time, mouse_delta, km, scene);
				break;
			}
			default:
				break;
		}
	}

	return false;
}


void Camera::updatePlayerCamera(int2& mouse_delta, Scene* scene)
{
	_yaw += mouse_delta.x * _mouse_speed * 0.5f;
	_pitch += mouse_delta.y * _mouse_speed * 0.5f;
	
	// Calculate pitch debt zoom.
	{
		// Pitch has lower limit of 0.0175f. If attempts to go below this, transfer to "debt".
		// Reason the limit is not 0.0f is that the voxel ray traversal has trouble with rays that are perfectly parallel to an axis.
		// Although those situations can still happen naturally, setting pitch to 0 guarantees it and it leaves obvious artifacts.
		static constexpr float pitch_limit{ 0.0175f };
		if (_pitch < pitch_limit)
		{
			// Move absolute value of pitch that is beyond limit to debt.
			if (_pitch < 0.0f)
			{
				_pitch_debt += pitch_limit + _pitch * -1.0f;
			}
			else
			{
				_pitch_debt += _pitch;
			}

			// Cap debt.
			_pitch_debt = fminf(_pitch_debt, _max_pitch_debt);

			// Set pitch to limit.
			_pitch = pitch_limit;
		}
		// As long as pitch debt has value, pitch sends all "earnings" to it.
		else if (_pitch > pitch_limit && _pitch_debt > 0.0f)
		{
			float transfer_amount = fminf(_pitch_debt, _pitch - pitch_limit);

			_pitch -= transfer_amount;
			_pitch_debt -= transfer_amount;
		}
		// Pitch has upper limit to avoid inversion.
		else if (_pitch > _pitch_limit)
		{
			_pitch = _pitch_limit;
		}

		// Value of debt determines how close camera zooms in.
		float progress{ _pitch_debt * _inverse_max_pitch_bag };
		_distance_to_target = lerp(_max_distance_to_target, _min_distance_to_target, progress);
	}

	// Calculate orientation.
	calculateOrientation();

	// Check there are no obstructions between camera and avatar.
	{
		static int water_triangle_id{ scene->_triangle_bvh._id };

		float ray_length{ length(_ahead * _distance_to_target) };
		Ray obstruction_check{ _look_at, -_ahead, 0, ray_length, false };
		scene->findNearestToPlayer(obstruction_check, water_triangle_id);

		// If obstruction, move camera to point.
		if (obstruction_check.t < ray_length)
		{
			_position = obstruction_check.IntersectionPoint() + (_ahead * Ray::_epsilon_offset);
		}
	}

	// Recalculate the virtual image plane.
	calculateVirtualImagePlane();
}


void Camera::updateTelescopeCamera(const float delta_time, int2& mouse_delta, const KeyboardManager& km, Scene* scene)
{
	_yaw -= mouse_delta.x * _mouse_speed * 0.5f;
	_telescope_pitch -= mouse_delta.y * _mouse_speed * 0.5f;

	// Pitch has upper limit to avoid inversion.
	{
		if (_telescope_pitch > _pitch_limit)
		{
			_telescope_pitch = _pitch_limit;
		}
		else if (_telescope_pitch < -_pitch_limit)
		{
			_telescope_pitch = -_pitch_limit;
		}
	}

	// Calculate new virtual image plane before sending zoom limiting ray.
	{
		// Calculate orientation.
		calculateOrientation();

		// Recalculate the virtual image plane.
		calculateVirtualImagePlane();
	}

	// Calculate the telescope zoom level.
	{
		// Limit the max zoom to not go through objects.
		static float2 midscreen{ make_float2(SCRWIDTH, SCRHEIGHT) * 0.5f };

		Ray zoom_limiting_ray{ getPickingRay(midscreen, 0) };
		scene->findNearest(zoom_limiting_ray);

		// Whatever distance the limiting ray reached, subtract epsilon so camera isn't not in object.
		// Comparison is used twice - calculate once.
		// Value is between [0, zoom limiting ray].
		float limit_ray_lower_cap{ fmaxf(0.0f, zoom_limiting_ray.t - 0.02f) };		
		
		// Max zoom value is between [limit ray lower cap, 100].
		float max_telescope_zoom{ fminf(limit_ray_lower_cap, 100.0f) };

		// Minimum zoom is only less than standard if the zoom limiting ray was smaller.
		float adjusted_min_zoom{ fminf(_min_telescope_zoom, limit_ray_lower_cap) };

		// Player-adjusted zoom.
		_telescope_zoom += delta_time * _zoom_speed * ((km.isPressed(Action::ZoomIn) ? 1.0f : 0.0f) + (km.isPressed(Action::ZoomOut) ? -1.0f : 0.0f));

		// Zoom value is between [0, 100] <- 100 is upper limit of max zoom.
		_telescope_zoom = fmaxf(adjusted_min_zoom, _telescope_zoom);
		_telescope_zoom = fminf(max_telescope_zoom, _telescope_zoom);
	}
}


void Camera::enableTelescope(const bool is_enabled)
{
	if (is_enabled)
	{
		_position = _look_at;
		_telescope_pitch = 0.0f;
		_yaw -= PI;
		_telescope_zoom = 0.0f;

		setMode(CamMode::FISHEYE);
	}
	else
	{
		_yaw += PI;
		setMode(CamMode::PINHOLE);
		setLookAt(_position);
	}
}


void Camera::setLookAt(const float3 look_at_position)
{
	if (_in_player_mode)
	{
		// Player position (unless in fisheye / telescope mode).
		_look_at = look_at_position;

		calculateOrientation();
	}
}


void Camera::setWorld(const float3& ahead, const float3& up, const float3& right)
{
	_world_ahead = ahead;
	_world_up = up;
	_world_right = right;
}


void Camera::calculateOrientation()
{
	switch (_cam_mode)
	{
		case CamMode::PINHOLE:
		{	
			float3 target_to_camera{
				_world_ahead * cosf(_yaw) * cosf(_pitch)
				+ _world_up * sinf(_pitch)
				+ _world_right * sinf(_yaw) * cosf(_pitch)
			};

			_position = _look_at + (target_to_camera * _distance_to_target);
			
			_ahead	= -target_to_camera;			
			_right	= normalize(cross(_world_up, _ahead));
			_up		= normalize(cross(_ahead, _right));
		
			// Needed because player avatar uses camera vectors as their own.
			// If regular _ahead is used, player will look into the ground, etc,
			// and move very slowly.
			_no_pitch_ahead = normalize(cross(_right, _world_up));
		
			break;
		}
		case CamMode::FISHEYE:
		{
			_ahead = float3{
				cosf(_yaw) * cosf(_telescope_pitch),
				sinf(_telescope_pitch),
				sinf(_yaw) * cosf(_telescope_pitch)
			};

			_right = normalize(cross(_world_up, _ahead));
			_up = normalize(cross(_ahead, _right));
		
			break;
		}
		default:
		{
			_ahead = normalize(_look_at - _position);
			_right = normalize(cross(_world_up, _ahead));
			_up = normalize(cross(_ahead, _right));
		
			break;
		}
	}
}


void Camera::calculateVirtualImagePlane()
{
	// Find bounding corners of plane that intersects frustum that will define the virtual image (vim).
	float3 vim_center{ _position + (2.0f * _ahead) };
	float3 vim_half_width{ _right * _vim_half_width };
	float3 vim_half_height{ _up * _vim_half_height };

	_top_left = vim_center - vim_half_width + vim_half_height;
	_top_right = vim_center + vim_half_width + vim_half_height;
	_bottom_left = vim_center - vim_half_width - vim_half_height;

	// Calculate edges along the vim.
	_u_vec = _top_right - _top_left;
	_v_vec = _bottom_left - _top_left;

	// [Credit] Big assist from Lynn (230137) on figuring out the z_vec!
	_z_vec = _ahead * length(0.5f * _v_vec);
}


float2 Camera::getUV(const float2& pixel) const
{
	static constexpr float dx = 1.0f / SCRWIDTH;
	static constexpr float dy = 1.0f / SCRHEIGHT;

	return { pixel.x * dx, pixel.y * dy };
}


float3 Camera::getVirtualPlanePixelPosition(const float2& pixel) const
{
	const float2 viewpoint{ getUV(pixel) };

	return { _top_left + (viewpoint.x * _u_vec) + (viewpoint.y * _v_vec) };
}


Ray Camera::getPickingRay(const float2 pixel, const uint source_voxel)
{
	return { _position, normalize(getVirtualPlanePixelPosition(pixel) - _position), source_voxel, false };
}


Ray Camera::getKillRay(const float2 pixel, const uint source_voxel) const
{
	return { _position, normalize(getVirtualPlanePixelPosition(pixel) - _position), source_voxel, length(_look_at - _position), false };
}


Ray Camera::getPrimaryRay(const float2 pixel, const uint source_voxel) const
{
	switch (_cam_mode)
	{
	case CamMode::PINHOLE:
	{
		return { _position, normalize(getVirtualPlanePixelPosition(pixel) - _position), source_voxel, false };
	}
	case CamMode::THIN_LENS:
	{
		// Find focus point based on direction.
		float3 position_to_pixel{ normalize(getVirtualPlanePixelPosition(pixel) - _position) };
		float t_to_focal_plane{ _focal_plane_distance / dot(_ahead, position_to_pixel) };
		float3 focus_point{ _position + position_to_pixel * t_to_focal_plane };

		// Find random point on lens.
		static constexpr float inverse_scrwidth{ 1.0f / SCRWIDTH };
		float2 lens_offset{ getRandomPointInPolygon() * inverse_scrwidth }; // Divide to get % to traverse the right/up vectors.
		float3 origin{ _position + (_right * lens_offset.x) + (_up * lens_offset.y) };

		return { origin, normalize(focus_point - origin), source_voxel, false };
	}
	case CamMode::FISHEYE:
	{
		constexpr static float radius{ SCRHEIGHT >> 1 };
		constexpr static float inverse_radius{ 1.0f / radius };
		constexpr static float radius2{ radius * radius };
		const static float2 center{ SCRWIDTH >> 1, SCRHEIGHT >> 1 };

		// If distance is greater than our circle, discard ray.
		float2 distance{ pixel - center };
		if (float distance_from_center{ sqrLength(distance) }; distance_from_center > radius2)
		{
			// Ray with 0 length will return black.
			return { _position, _position, source_voxel, 0.0f, false };
		}

		// [Credit] Lynn (230137) had to reteach me how to solve for z and helped think of best way to make the z_vec.
		float2 difference{ pixel - center };
		float z{ sqrt(radius2 - (difference.x * difference.x) - (difference.y * difference.y)) };

		float z_percent{ z * inverse_radius };
		float3 point_on_sphere{ getVirtualPlanePixelPosition(pixel) + (_z_vec * z_percent) };

		return { _position + (_telescope_zoom * _ahead), normalize(point_on_sphere - _position), source_voxel, false };
	}
	default:
	{
		assert(false && "Used camera mode enum that does not exist.");
		return { _position, normalize(getVirtualPlanePixelPosition(pixel) - _position), source_voxel, false };
	}
	}
}


void Camera::setFOV(float horizontal_degrees)
{
	_old_fov = _fov;

	_fov = min(horizontal_degrees, 179.0f);

	float half_fov_rad = _fov * 0.5f * PI / 180.0f;

	_vim_half_width = tanf(half_fov_rad) * 0.5f; // tan(half_fov) / (ahead * 2)
	_vim_half_height = _vim_half_width / _aspect_ratio;
}


void Camera::setApertureRadius(float radius)
{
	_aperture_radius = radius;
	setupApertureShape();
}


float2 Camera::getRandomCircleSample() const
{
	static float radius{ 0.5f };
	static float diameter{ radius * 2.0f };

	return { diameter * RandomFloat() - radius, diameter * RandomFloat() - radius };
}


float2 Camera::getRandomPointInPolygon() const
{
	// Use Point-in-Polygon algorithm to find if random point in our shape.
	float max_value{ 2.0f * _aperture_radius };
	while (true)
	{
		float2 point{ max_value * RandomFloat(), max_value * RandomFloat() };

		if (getIsPointInPolygon(point))
		{
			return { point - _aperture_radius }; // Convert from [0, 2r] to [-r, r] range.
		}
	}
}


void Camera::setupApertureShape()
{
	// Rebuild if enough points to make a polygon.
	if (_polygon_count > 2)
	{
		// Distribute points equally.
		float origin{ _aperture_radius };
		float angle{ 0.0f };
		float angle_increment = PI * 2.0f / _polygon_count;

		for (int vertex = 0; vertex < _polygon_count; ++vertex)
		{
			_aperture_vertices[vertex] = make_float2(origin + _aperture_radius * cosf(angle), origin + _aperture_radius * sinf(angle));
			angle += angle_increment;
		}
	}
}


// Point-in-Polygon algorithm by James Halliday.
// [Credit] https://observablehq.com/@tmcw/understanding-point-in-polygon
const bool Camera::getIsPointInPolygon(float2 point) const
{
	assert(_polygon_count > 2 && "Polygon has less than 3 points");

	bool inside{ false };
	int vertices_max_index{ _polygon_count - 1 };
	int vertices_size{ _polygon_count };

	float x{ point.x };
	float y{ point.y };

	for (int i = 0, j = vertices_max_index; i < vertices_size; j = i++)
	{
		// Get 2 points that make up the start and end of a line segment.
		float xi = _aperture_vertices[i].x;
		float yi = _aperture_vertices[i].y;

		float xj = _aperture_vertices[j].x;
		float yj = _aperture_vertices[j].y;

		// Find if a line from our point towards +x crosses the line segment.
		bool intersect =
			((yi > y) != (yj > y)) // Check if the line segment extends above and below our point.
			&& // Next, draw a horizontal ray through our point and to the line segment.
			(x < (xj - xi) * (y - yi) / (yj - yi) + xi); // If out point is to the left of the intersection, count the intersection.

		// Flip this bool every time there is an intersection to the right of our point.
		// An odd # of intersections = outside the polygon; even # = inside.
		if (intersect)
		{
			inside = !inside;
		}
	}

	return inside;
}


void Camera::moveFocalDistanceTo(float target_focal_distance)
{
	_is_changing_focus = true;
	_target_focal_plane_distance = target_focal_distance;
}