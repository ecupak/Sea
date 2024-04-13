#pragma once


namespace Tmpl8 {
	class Scene;
}


enum class CamMode : int
{
	PINHOLE = 0,
	THIN_LENS,
	FISHEYE,
};


class Camera
{
public:
	// Ctor.
	Camera();


	// Handles input and precalculates new virtual screen positioning.
	const bool update(const float delta_time, const KeyboardManager& km, Scene* scene);

	Ray getPrimaryRay(const float2 pixel, const uint source_voxel) const;

	Ray getKillRay(const float2 pixel, const uint source_voxel) const;
	
	Ray getPickingRay(const float2 pixel, const uint source_voxel);

	void setMode(const CamMode mode);

	void moveFocalDistanceTo(const float target_focal_distance);

	void setFOV(const float horizontal_degrees);

	void setApertureRadius(const float radius);

	void setLookAt(const float3 look_at_position);	
	void setWorld(const float3& ahead, const float3& up, const float3& right);

	void enableTelescope(const bool is_enabled);

	bool prepareToFlip();

	Camera retire();

	float3 _no_pitch_ahead{ 1.0f };
	
	float3 _position{ 0.0f };
	
	float3 _ahead{ 1.0f };
	float3 _right{ 1.0f };
	float3 _up{ 1.0f };
	float3 _look_at{ 1.0f };
	
	float3 _world_ahead{ 0.0f, 0.0f, 1.0f };
	float3 _world_up{ 0.0f, 1.0f, 0.0f };
	float3 _world_right{ 1.0f, 0.0f, 0.0f };
	
	float3 _top_left{ 0.0f };
	float3 _top_right{ 0.0f };
	float3 _bottom_left{ 0.0f };
	float3 _u_vec{ 1.0f, 0.0f, 0.0f };
	float3 _v_vec{ 0.0f, -1.0f, 0.0f };
	float3 _z_vec{ 0.0f, 0.0f, 1.0f };
	
	float3 _top_normal{ 1.0f };
	float3 _left_normal{ 1.0f };
	float3 _right_normal{ 1.0f };
	float3 _bottom_normal{ 1.0f };
	
	std::array<float2, 7> _aperture_vertices;
	
	int2 _previous_mouse_position{ 0, 0 };

	CamMode _cam_mode{ CamMode::PINHOLE };

	bool _in_player_mode{ true };
	bool _is_mouse_initialized{ false };

	float _min_distance_to_target{ 0.1f };
	float _max_distance_to_target{ 0.3f };
	float _distance_to_target{ _max_distance_to_target };
	float _yaw{ 0.0f };
	float _pitch{ PI / 8.0f };
	float _pitch_limit{ PI / 2.f * 0.9f };
	float _pitch_debt{ 0.0f };
	float _max_pitch_debt{ PI / 8.0f };
	float _inverse_max_pitch_bag{ 1.0f / _max_pitch_debt };
	float _telescope_pitch{ 0.0f };
	float _mouse_speed{ PI / 360.0f };

	// Camera position and orientation.
	float _speed{ 1.3f };
	
	// Get world vectors from player.
	bool _is_shifting_worlds{ false };

	// Virtual screen position and edges.
	float _vim_half_height{ 1.0f };
	float _vim_half_width{ 1.0f };

	// Aspect ratio & field of view.
	float _fov{ 130.0f };
	float _fov_rad{ 1.0f };
	float _aspect_ratio{ 1.0f };
	float _old_fov{ 1.0f };

	// Focal distance.
	float _focal_plane_distance{ 2.0f };
	float _target_focal_plane_distance{ 2.0f };
	float _focal_change_step_percent{ 0.5f };
	bool _is_changing_focus{ false };

	// Frustrum normals for reprojection.
	// Aperture.
	int _polygon_count{ 5 };
	float _aperture_radius{ 1.0f };

	// Fisheye
	float _z_length{ 1.0f };
	float _telescope_zoom{ 0.0f };
	float _min_telescope_zoom{ 0.06f };
	float _zoom_speed{ 0.5f };

	// Update/Input flow.
	bool _is_save_ready{ true };
	bool _is_recalculation_needed{ false };
	bool _can_flip{ true };
	float _flip_indicator{ 1.0f };

	// Do not intersect this object for camera obstruction checks.
	uint _ship_id{ 0 };

private:
	void updatePlayerCamera(int2& mouse_delta, Scene* scene);
	void updateTelescopeCamera(const float delta_time, int2& mouse_delta, const KeyboardManager& km, Scene* scene);

	void calculateOrientation();
	void calculateVirtualImagePlane();

	float2 getUV(const float2& pixel) const;
	float3 getVirtualPlanePixelPosition(const float2& pixel) const;

	void setupApertureShape();
	float2 getRandomPointInPolygon() const;
	const bool getIsPointInPolygon(float2 point) const;

	float2 getRandomCircleSample() const;
};