#include "precomp.h"
#include "renderer.h"

//#include <execution>

Renderer::~Renderer() {}


// -----------------------------------------------------------
// Initialize the renderer
// -----------------------------------------------------------

// Setup multithreading.
#ifdef NDEBUG
#define MULTI_THREADED 1
#else
#define MULTI_THREADED 0	
#endif

void Renderer::Init()
{                  
	scene.generateScene(Scene::Stage::WORLD);

	// Prevent camera from considering ship as obstacle.
	_camera._ship_id = scene._player._ship._bvh._id;

	// Must set to 1 lower and then back to normal. No idea why.
	Ray::setEpsilon(2);
	Ray::setEpsilon(5);

	// Create subpixel offset using Halton sequence.
	// [Credit] Ray Tracing Gems II
	{
		float x_offsets[_HALTON_SAMPLE_SIZE];
		float y_offsets[_HALTON_SAMPLE_SIZE];

		_halton_sequencer.generateSamples(_HALTON_SAMPLE_SIZE, 2, x_offsets);
		_halton_sequencer.generateSamples(_HALTON_SAMPLE_SIZE, 3, y_offsets);

		for (int i = 0; i < _HALTON_SAMPLE_SIZE; ++i)
		{
			_subpixel_positions[i].x = x_offsets[i];
			_subpixel_positions[i].y = y_offsets[i];
		}
	}

	// Create buffers of aligned memory.
	constexpr static size_t size_of_array3{ SCRWIDTH * SCRHEIGHT * sizeof(float3) };
	constexpr static size_t size_of_array4{ SCRWIDTH * SCRHEIGHT * sizeof(float4) };
	
	_accumulator = static_cast<float3*>(MALLOC64(size_of_array3));
	if (_accumulator) { memset(_accumulator, 0, size_of_array3); }
	
	_albedo_buffer = static_cast<float3*>(MALLOC64(size_of_array3));
	if (_albedo_buffer) { memset(_albedo_buffer, 0, size_of_array3); }
	
	_pixel_new_buffer = static_cast<float3*>(MALLOC64(size_of_array3));
	if (_pixel_new_buffer) { memset(_pixel_new_buffer, 0, size_of_array3); }

	_pixel_reprojected_buffer = static_cast<float3*>(MALLOC64(size_of_array3));
	if (_pixel_reprojected_buffer) { memset(_pixel_reprojected_buffer, 0, size_of_array3); }
	
	_pixel_history_buffer = static_cast<float3*>(MALLOC64(size_of_array3));
	if (_pixel_history_buffer) { memset(_pixel_history_buffer, 0, size_of_array3); }
	
	_ray_buffer = new Ray[SCRWIDTH * SCRHEIGHT];
	memset(_ray_buffer, 0, SCRWIDTH * SCRHEIGHT * sizeof(Ray));


	// Try to load a camera.
	if (false)
	{
		FILE* f = fopen("camera.bin", "rb");
		if (f)
		{
			fread(&_camera, 1, sizeof(Camera), f);
			fclose(f);
		}
	}

	// Prime the camera reprojection packet.
	_retired_camera = _camera;
}


// -----------------------------------------------------------
// Main application tick function - Executed once per frame
// -----------------------------------------------------------
void Renderer::Tick(float delta_time)
{
	// Starting material and screen midpoints as int and float.
	static uint air_material{ 0 };
	static int2 midscreeni{ SCRWIDTH >> 1, SCRHEIGHT >> 1 };
	static float2 midscreenf{ static_cast<float2>(midscreeni) };

	// Pixel loop
	Timer t;

	// Update keyboard.
	_km.update();

	// Update scene (player and islands).
	scene.update(delta_time, _km);

	// Update camera based on input.
	{
		_retired_camera = _camera.retire();	
		if (_camera.update(delta_time, _km, &scene) && _use_accumulator)
		{
			resetAccumulator();
		}

		_world_side_modifier = _camera._position.y > 0.5f ? 1.0f : 0.0f;
		
		static Light& sunlight{ scene._light_list._lights[0] };
		sunlight._intensity = _world_side_modifier;
	}
	
	// Track how many frames have accumulated so far.
	_frame_count = _frame_count + 1.0f;
	float inverse_accumulated_frames{ 1.0f / _frame_count };

	// Refit/Rebuild TLAS & BLAS.
	scene.refitAS();

	// Remove obstructing voxels.
	{
		static int2 mid{ SCRWIDTH >> 1, SCRHEIGHT >> 1 }; // midscreen
		static int o{ 10 };	 // offset
		static float2 erasure_ray_coordinates[21]{
			make_float2(mid.x - o * 2, mid.y - o),	make_float2(mid.x - o, mid.y - o),		make_float2(mid.x, mid.y - o),		make_float2(mid.x + o, mid.y - o),		make_float2(mid.x + o * 2, mid.y - o),
			make_float2(mid.x - o * 2, mid.y),		make_float2(mid.x - o, mid.y),			make_float2(mid.x, mid.y),			make_float2(mid.x + o, mid.y),			make_float2(mid.x + o * 2, mid.y),
			make_float2(mid.x - o * 2, mid.y + o),	make_float2(mid.x - o, mid.y + o),		make_float2(mid.x, mid.y + o),		make_float2(mid.x + o, mid.y + o),		make_float2(mid.x + o * 2, mid.y + o),
			make_float2(mid.x - o * 2, mid.y + o * 2),	make_float2(mid.x - o, mid.y + o * 2),	make_float2(mid.x, mid.y + o * 2),	make_float2(mid.x + o, mid.y + o * 2),	make_float2(mid.x - o * 2, mid.y + o * 2),
																							make_float2(mid.x, mid.y + o * 3),
		};

		//shootErasureRays(erasure_ray_coordinates);
	}

	// Primary ray generation.
	shootPrimaryRays();

	// Reprojection.
	if (_use_reprojection)
	{
		applyReprojection();		
	}
	else // Transfer from ray data to bilinear interpolation data.
	{
		memcpy(_pixel_reprojected_buffer, _pixel_new_buffer, SCRWIDTH * SCRHEIGHT * sizeof(float3));
	}

	// Denoising.
	if (_use_denoiser)
	{
		applyDenoising();
	}
	else // Transfer from bilinear interpolation data to pixel history data.
	{
		memcpy(_pixel_history_buffer, _pixel_reprojected_buffer, SCRWIDTH * SCRHEIGHT * sizeof(float3));
	}

	// Draw to screen.
	{
#if MULTI_THREADED
#pragma omp parallel for schedule(dynamic)
#endif
		for (int y = 0; y < SCRHEIGHT; ++y)
		{
			const uint pitch{ static_cast<uint>(y * SCRWIDTH) };
			{
				switch (_use_accumulator)
				{
					case true:
					{
						for (int x = 0; x < SCRWIDTH; ++x)
						{
							const uint pixel_index{ x + pitch };

							_accumulator[pixel_index] += _albedo_buffer[pixel_index] * _pixel_history_buffer[pixel_index];

							float3 final_pixel = _accumulator[pixel_index] * inverse_accumulated_frames;
							float3 tonemapped_pixel{ tonemap(final_pixel) };
							screen->pixels[pixel_index] = float3_to_uint(tonemapped_pixel);
						}


						break;
					}
					case false:
					{
						for (int x = 0; x < SCRWIDTH; ++x)
						{
							const uint pixel_index{ x + pitch };

							float3 final_pixel;
							final_pixel = _albedo_buffer[pixel_index] * _pixel_history_buffer[pixel_index];

							float3 tonemapped_pixel{ tonemap(final_pixel) };
							screen->pixels[pixel_index] = float3_to_uint(tonemapped_pixel);
						}

						break;
					}
					
				}
			}
		}
	}

	// Display audio card.
	showAudioCard(delta_time);

	// Restore voxels "erased" by erasure ray.
	//scene.restoreVoxels();

#ifdef _DEBUG
	// Mark middle of screen with box.
	screen->Box(_focal_point.x - 1, _focal_point.y - 1, _focal_point.x + 1, _focal_point.y + 1, 0xFFFFFF);
#endif

	// Performance report - running average - ms, MRays/s
	static float avg{ 10 };
	static float alpha{ 1 };
	avg = (1.0f - alpha) * avg + alpha * t.elapsed() * 1000.0f;
	if (alpha > 0.05f)
	{
		alpha *= 0.5f;
	}

	float fps{ 1000.0f / avg };
	float rps{ (SCRWIDTH * SCRHEIGHT) / avg };
	printf("%5.2fms (%.1ffps) - %.1fMrays/s\n", avg, fps, rps / 1000);
}


void Renderer::resetAccumulator()
{
	memset(_accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
	_frame_count = 0.0f;
}


void Renderer::shootErasureRays(float2 coordinates[])
{
	static uint air_material{ 0 };

	// Base values are from smallest screen with no stretch.
	// Stretch simply applies multiplier to values.
	// Must find proportion to other screen sizes.

	for (int i = 0; i < 21; ++i)
	{
		Ray kill_ray = _camera.getKillRay(coordinates[i], air_material);
		scene.eraseVoxels(kill_ray);
	}
}


void Renderer::shootPrimaryRays()
{
	static uint air_material{ 0 };

	// Get subpixel position for this frame.
	float2 subpixel_offset{ 0.0f, 0.0f };
	if (_use_antialiasing)
	{
		subpixel_offset = _subpixel_positions[_offset_index];
		_offset_index = (_offset_index + 1) % _HALTON_SAMPLE_SIZE;
	}

#if _DEBUG
	int pixel_index{ _focal_point.x + _focal_point.y * SCRWIDTH };

	Ray debug_ray = _camera.getPickingRay(make_float2(_focal_point), air_material);

	const TraceRecord record{ trace(debug_ray, _max_depth) };

	_albedo_buffer[pixel_index] = record._albedo;
	_ray_buffer[pixel_index] = debug_ray;
	_pixel_new_buffer[pixel_index] = record._light;
#else

#if MULTI_THREADED
#pragma omp parallel for schedule(dynamic)
#endif
	for (int y = 0; y < SCRHEIGHT; y += TILE_SIZE)
	{
		for (int x = 0; x < SCRWIDTH; x += TILE_SIZE)
		{
			for (int v = 0; v < TILE_SIZE; ++v)
			{
				const int py{ y + v };
				const uint pitch{ static_cast<uint>(py * SCRWIDTH) };

				for (int u = 0; u < TILE_SIZE; ++u)
				{
					const int px{ x + u };
					uint pixel_index{ px + pitch };

					Ray ray{ _camera.getPrimaryRay(make_float2(px + subpixel_offset.x, py + subpixel_offset.y), air_material) };
					TraceRecord record{ trace(ray, _max_depth) };

					//if (ray._distance_underwater > 0.0f)
					{
						// Apply Beer's Law. Use distance underwater to determine absorption amount.
						float3 beers_absorbance{ getAbsorption(scene._triangles[0]._data, ray._distance_underwater) };

						record._light = record._light * beers_absorbance;
					}

					_albedo_buffer[pixel_index] = record._albedo;
					_ray_buffer[pixel_index] = ray;
					_pixel_new_buffer[pixel_index] = record._light;
				}
			}
		}
	}
#endif
}


void Renderer::applyReprojection()
{
#if MULTI_THREADED
#pragma omp parallel for schedule(dynamic)
#endif

#ifdef _DEBUG
#define EARLYOUT return
	{
		{
			int y{ _focal_point.y };
			int x{ _focal_point.x };
			const int pixel_index{ x + y * SCRWIDTH };
#else
#define EARLYOUT continue
	for (int y = 0; y < SCRHEIGHT; ++y)
	{
		const int pitch{ y * SCRWIDTH };

		for (int x = 0; x < SCRWIDTH; ++x)
		{
			const int pixel_index{ x + pitch };
#endif
			float3& new_sample{ _pixel_new_buffer[pixel_index] };
			Ray& new_ray{ _ray_buffer[pixel_index] };

			// Get pixel position last frame (in UV coordinates).
			float2 old_uv;
			{
				// First, determine if the point has history.
				float3 new_point{ new_ray.IntersectionPoint() };
				
				float3 old_camera_to_new_point{ new_point - _retired_camera._position };
				static float safe_zone_padding{ 0.001f }; // We do not want the ray to collide with the point in question.
				Ray old_ray{ _retired_camera._position, old_camera_to_new_point, length(old_camera_to_new_point) - safe_zone_padding, false };
				
				if (new_ray.t == Ray::t_max) // || scene.isOccluded(old_ray))
				{
					// This is the skydome, or we did not see this point last frame - discard history and start fresh.
					_pixel_reprojected_buffer[pixel_index] = new_sample;
					EARLYOUT;
				}

				// Second, get the pixel's old UV position.
				float3 old_ray_direction_normalized{ normalize(old_ray.D) };

				const float distance_from_top{ dot(old_ray_direction_normalized, _retired_camera._top_normal) };
				const float distance_from_left{ dot(old_ray_direction_normalized, _retired_camera._left_normal) };
				const float distance_from_right{ dot(old_ray_direction_normalized, _retired_camera._right_normal) };
				const float distance_from_bottom{ dot(old_ray_direction_normalized, _retired_camera._bottom_normal) };

				old_uv.x = distance_from_left / (distance_from_left + distance_from_right);
				old_uv.y = distance_from_top / (distance_from_top + distance_from_bottom);

				if (old_uv.x < 0 || old_uv.x >= 1.0f || old_uv.y < 0 || old_uv.y >= 1.0f)
				{
					// Old position was not in view frustrum last frame - it has no history.
					_pixel_reprojected_buffer[pixel_index] = new_sample;
					EARLYOUT;
				}
			}

			// Must do weird rounding to prevent "pixel drift" where then wrong history pixel is sampled.
			float2 history_pixel_position{ ((old_uv.x * SCRWIDTH) + 0.5f), ((old_uv.y * SCRHEIGHT) + 0.5f) };
			
			// Get the history sample (after appling bilinear interpolation to it).
			float3 history_sample{ getHistorySample(history_pixel_position)};			

			// Clamp sample.
			applyColorClamping(history_sample, new_sample, { x, y });

			// Mix new sample and history sample.
			// [Credit] Lynn-inspired.
			float history_weight{ 0.9f };
			switch (MaterialList::GetType(new_ray._hit_data))
			{
			case MaterialType::GLASS:
			case MaterialType::WATER:
				history_weight = 0.1f;
				break;
			case MaterialType::NON_METAL:
				history_weight = 0.99f;
				break;
			default:
				break;
			}

			_pixel_reprojected_buffer[pixel_index] = lerp(new_sample, history_sample, history_weight);
		}
	}
}


bool Renderer::findPreviousPixelPosition(const uint current_pixel_index, float2& previous_pixel) const
{
	// Cast ray from old camera to current point.
	static constexpr float ray_truncation{ 0.0006f };

	const Ray& new_ray{ _ray_buffer[current_pixel_index] };

	const float3 reprojected_ray_direction{ new_ray.IntersectionPoint() - _retired_camera._position };
	const float reprojected_ray_distance{ length(reprojected_ray_direction) - ray_truncation };
	Ray reprojection_ray{ _retired_camera._position, reprojected_ray_direction, reprojected_ray_distance, false };
	
	if (scene.isOccluded(reprojection_ray))
	{
		// Can't see new point - reset history.
		return false;
	}

	// Calculate the % horizontal and vertical position of where the current pixel was last frame.
	const float3 direction_normalized{ reprojection_ray.D };

	const float top_delta{		dot(direction_normalized, _retired_camera._top_normal) };
	const float left_delta{		dot(direction_normalized, _retired_camera._left_normal) };
	const float right_delta{	dot(direction_normalized, _retired_camera._right_normal) };
	const float bottom_delta{	dot(direction_normalized, _retired_camera._bottom_normal) };

	const float x_percent{ left_delta / (left_delta + right_delta) };
	const float y_percent{ top_delta / (top_delta + bottom_delta) };


	if (x_percent < 0.0f || x_percent >= 1.0f || y_percent < 0.0f || y_percent >= 1.0f)
	{
		// New point is outside frustum of previous camera - reset history.
		return false;
	}

	previous_pixel = float2{ x_percent * _screenf.x, y_percent * _screenf.y };

	//printf("UV: %.4f, %.4f -> SCR: %.4f, %.4f | ", x_percent, y_percent, previous_pixel.x, previous_pixel.y);

	return true;
}


float3 Renderer::getHistorySample(const float2& history_pixel_position) const
{
	// Find top-left corner of sampling square and identify the other pixels in the sampling square.
	const float2 sampling_square_top_left_f{ history_pixel_position - 0.5f };
	const int2 sampling_square_top_left_i{ static_cast<int>(sampling_square_top_left_f.x), static_cast<int>(sampling_square_top_left_f.y) };

	const int2 samples[4] = {
		make_int2(sampling_square_top_left_i.x,		sampling_square_top_left_i.y		), // top left
		make_int2(sampling_square_top_left_i.x + 1,	sampling_square_top_left_i.y		), // top right
		make_int2(sampling_square_top_left_i.x,		sampling_square_top_left_i.y + 1	), // bottom left
		make_int2(sampling_square_top_left_i.x + 1,	sampling_square_top_left_i.y + 1	)  // bottom right
	};

	// Find weight of each position.
	// ... Intrusion is the fractional value of the corner position.
	const float2 intrusion{ make_float2(
		sampling_square_top_left_f.x - floorf(sampling_square_top_left_f.x), // float - int of same number:
		sampling_square_top_left_f.y - floorf(sampling_square_top_left_f.y)  // 3.017 - 3 = 0.017
	)};	

	// ... Calculate weight of each overlap by the area of intrusion.
	float weights[4]{
		(1.0f - intrusion.x) * (1.0f - intrusion.y), // top left
		(       intrusion.x) * (1.0f - intrusion.y), // top right
		(1.0f - intrusion.x) * (       intrusion.y), // bottom left
											  0.0f   // bottom right - calculated on next line
	};
	weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

	// ... If sample pixel

	// ... Adjust weights so if any are 0 they still add to 1 while maintaining distribution.
	// Weight is 0 if it is not on screen (pixel is on border).
	// [Credit] Lynn
	float total_weight{ 0.0f };
	for (int i = 0; i < 4; ++i)
	{
		const int2& sample{ samples[i] };

		if (sample.x < 0 || sample.x >= SCRWIDTH || sample.y < 0 || sample.y >= SCRHEIGHT)
		{
			// Offscreen, no weight.
			weights[i] = 0.0f;
		}

		total_weight += weights[i];
	}
	const float inverse_total_weight{ 1.0f / total_weight };

	// Get full bilinearly interpolated history sample.
	float3 history_sample{ 0.0f };
	for (int i = 0; i < 4; ++i)
	{
		const int2& sample{ samples[i] };

		if (weights[i] > 0.0f) // TODO: Remove this check. 0 weight will math itself out.
		{
			float3 history_pixel{ _pixel_history_buffer[sample.x + sample.y * SCRWIDTH] };
			history_sample += history_pixel * weights[i] * inverse_total_weight;
		}
	}

	return history_sample;	
}


// [Credit] All RGB to YCoCg conversions and the color clamping: https://www.shadertoy.com/view/4dSBDt
float3 Renderer::RGB_to_YCoCg(float3 rgb)
{
	constexpr static float modifier{ 0.5f * 256.0f / 255.0f };

	float Y{  dot(rgb, float3( 1.0f, 2.0f,  1.0f)) * 0.25f };
	float Co{ dot(rgb, float3( 2.0f, 0.0f, -2.0f)) * 0.25f + modifier };
	float Cg{ dot(rgb, float3(-1.0f, 2.0f, -1.0f)) * 0.25f + modifier };
	
	return { Y, Co, Cg };
}


float3 Renderer::YCoCg_to_RGB(float3 YCoCg)
{
	constexpr static float modifier{ 0.5f * 256.0f / 255.0f };

	float Y{  YCoCg.x };
	float Co{ YCoCg.y - modifier };
	float Cg{ YCoCg.z - modifier };
	
	float R{ Y + Co - Cg };
	float G{ Y      + Cg };
	float B{ Y - Co - Cg };

	return { R, G, B };
}


void Renderer::applyColorClamping(float3& color_to_clamp, float3 reference_color, int2 reference_color_position) const
{
	const static int offset_count{ 8 };

	int2 offsets[offset_count]{
		{-1, -1}, { 0, -1}, { 1, -1},
		{-1,  0}, /*blank*/ { 1,  0},
		{-1,  1}, { 0,  1}, { 1,  1}
	};

	reference_color = RGB_to_YCoCg(reference_color);

	float3 color_average{ reference_color };
	float3 color_variance{ reference_color * reference_color };

	// Sample all pixel around new pixel position. Skip those that are outside the viewport.
	int sample_count{ 1 }; // <- Lynn's catch.
	for (auto offset : offsets)
	{
		int2 offset_position{ reference_color_position + offset};

		if (offset_position.x < 0 || offset_position.x >= SCRWIDTH || offset_position.y < 0 || offset_position.y >= SCRHEIGHT)
		{
			continue;
		}

		float3 staged_YCoCg_color = RGB_to_YCoCg(_pixel_new_buffer[offset_position.x + (offset_position.y * SCRWIDTH)]);
		color_average += staged_YCoCg_color;
		color_variance += staged_YCoCg_color * staged_YCoCg_color;
		++sample_count;
	}

	// Average results.
	float inverse_offset_count{ 1.0f / static_cast<float>(sample_count) };
	color_average *= inverse_offset_count;
	color_variance *= inverse_offset_count;

	// Get sigma.
	const static float3 zero3{ 0.0f };
	float3 sigma_input{ color_variance - (color_average * color_average) };
	float3 sigma_max_input{ fmaxf(zero3, sigma_input) };
	float3 sigma{ sqrtf(sigma_max_input.x), sqrtf(sigma_max_input.y), sqrtf(sigma_max_input.z) };

	// Find clamping min/max values.
	const static float g_color_box_sigma{ 0.75f };
	float3 color_min{ color_average - g_color_box_sigma * sigma };
	float3 color_max{ color_average + g_color_box_sigma * sigma };

	// Clamp, convert, ensure valid range.
	color_to_clamp = RGB_to_YCoCg(color_to_clamp);

	color_to_clamp = clamp(color_to_clamp, color_min, color_max);
	color_to_clamp = YCoCg_to_RGB(color_to_clamp);
	color_to_clamp = fmaxf(zero3, color_to_clamp);
}


void Renderer::applyDenoising()
{
	static int range{ 2 };

#ifdef _DEBUG
	const int pixel_index{ _focal_point.x + _focal_point.y * SCRWIDTH };

	applySeperableBilinearFiltering(pixel_index, _focal_point, range, { 1, 0 }, _pixel_reprojected_buffer, _pixel_denoise_buffer);
	applySeperableBilinearFiltering(pixel_index, _focal_point, range, { 0, 1 }, _pixel_denoise_buffer, _pixel_history_buffer);
#else

#if MULTI_THREADED
#pragma omp parallel for schedule(dynamic)
#endif
	for (int y = 0; y < SCRHEIGHT; ++y)
	{
		const int pitch{ y * SCRWIDTH };

		for (int x = 0; x < SCRWIDTH; ++x)
		{
			const int pixel_index{ x + pitch };

			applySeperableBilinearFiltering(pixel_index, { x, y }, range, { 1, 0 }, _pixel_reprojected_buffer, _pixel_denoise_buffer);
		}
	}

#if MULTI_THREADED
#pragma omp parallel for schedule(dynamic)
#endif
	for (int x = 0; x < SCRWIDTH; ++x)
	{
		for (int y = 0; y < SCRHEIGHT; ++y)
		{
			const int pixel_index{ x + (y * SCRWIDTH) };

			applySeperableBilinearFiltering(pixel_index, { x, y }, range, { 0, 1 }, _pixel_denoise_buffer, _pixel_history_buffer);
		}
	}
#endif
}


void Renderer::applySeperableBilinearFiltering(const int current_pixel_index, const int2 current_pixel_origin, const int range, const int2 axis, float3* read_from, float3* write_to)
{
	#ifndef NDEBUG
	bool is_focal_point{false};
	is_focal_point = (current_pixel_origin.x == _focal_point.x) && (current_pixel_origin.y == _focal_point.y);
	if (is_focal_point)
	{
		int stop = 3;
	}
	#endif

	// Sample to the left and right of the origin.
	// Done in 2 passes - stepping away from the origin each time.
	int direction_indicator{ 1 };

	// Ray of the origin.
	Ray main_ray{ _ray_buffer[current_pixel_index] };

	// Results that will modify the color to filter.
	float total_weight{ 1.0f };
	float3 average_color{ read_from[current_pixel_index] };

	// Grow outwards from origin to range in 2 passes.
	for (int p = 0; p < 2; ++p)
	{
		for (int r = 0; r < range; ++r)
		{
			int sample_offset{ (r + 1) * direction_indicator };
			int2 sample_position{ current_pixel_origin.x + (sample_offset * axis.x), current_pixel_origin.y + (sample_offset * axis.y) };

			// Confirm position is on screen.
			// TODO: could remove this check by adding range_min, range_max parameters and using valid ranges based on known x position of origin.
			if (sample_position.x < 0 || sample_position.x >= SCRWIDTH || sample_position.y < 0 || sample_position.y >= SCRHEIGHT)
			{
				continue;
			}

			// Clamp color.
			static float squared_max_illumination_magnitude{ powf(5.0f, 2.0f) };
			float3 read_color{ read_from[sample_position.x + (sample_position.y * SCRWIDTH)] };
			{
				const float squared_magnitude{ sqrLength(read_color) };

				if (squared_magnitude > squared_max_illumination_magnitude)
				{
					continue;
				}
			}

			// Determine if this sample should be used.			
			if (const float weight{ getCommonPropertiesWeighting(main_ray, sample_position) };
				weight > 0.0f)
			{
				// Add color to average.
				average_color += read_from[sample_position.x + (sample_position.y * SCRWIDTH)] * weight;
				total_weight += weight;
			}
			else { break; }
		}

		direction_indicator *= -1;
	}	

	write_to[current_pixel_index] = average_color / total_weight;
}


float Renderer::getCommonPropertiesWeighting(const Ray& main_ray, const int2& other_position)
{
	float weight{ 1.0f };

	// Don't denoise the skydome.
	if (main_ray.t == Ray::t_max)
	{
		return 0.0f;
	}

	// Adjust denoising amount based on pixel material.
	switch (MaterialList::GetType(main_ray._hit_data))
	{
	case MaterialType::METAL:
		return 1.0f;
	case MaterialType::GLASS:
	case MaterialType::WATER:
		return 0.001f;
	default:
		break;
	}

	// Get the other ray. Used in the next 3 comparisons.
	const Ray other_ray{ _ray_buffer[other_position.x + other_position.y * SCRWIDTH] };

	// If material indexes are different, do not include in average.
	if (MaterialList::GetIndex(main_ray._hit_data) != MaterialList::GetIndex(other_ray._hit_data))
	{
		return 0.0f;
	}

	// Use the intersection points to adjust weights. Do not keep comparison weight if:
	// - main hit a sphere or triangle and other did not, or
	// - main and other intersection points do not share a plane
	{
		const float3 main_intersection{ main_ray.IntersectionPoint() };
		const float3 other_intersection{ other_ray.IntersectionPoint() };

		switch (main_ray._id)
		{
		case -2: // Spheres.
			if (other_ray._id != -2)
			{
				return 0.0f;
			}
			break;
		case -1: // Triangles.
			if (other_ray._id != -1)
			{
				return 0.0f;
			}
			break;
		default: // Voxels.
			{
				float3 diff{ main_intersection - other_intersection };

				static float comparison_epsilon{ 0.00000001f };
				if (!(diff.x * diff.x < comparison_epsilon || diff.y * diff.y < comparison_epsilon || diff.z * diff.z < comparison_epsilon))
				{
					return 0.0f;
				}
				break;
			}
		}
	}

	// Compare normals between the ray and adjust weight based on that with a Guassian curve.
	// [Credit] Lynn (230137)
	{
		float inverse_double_sigma_squared{ 1.0f / 2.0f * _sigma * _sigma };

		const float cos_theta{ dot(main_ray.normal, other_ray.normal) };

		weight *= expf(-(cos_theta * cos_theta) * inverse_double_sigma_squared);
	}

	return weight;
}



// -----------------------------------------------------------
// Evaluate light transport
// -----------------------------------------------------------
TraceRecord Renderer::trace(Ray& incident_ray, int depth)
{
	// Check if depth has been exceeded.
	if (depth == 0)
	{
		return { 0.0f, 0.0f };
	}

	// Traverse voxel world until we hit non-air material.
	if (scene.findNearest(incident_ray))
	{
		switch (MaterialList::GetType(incident_ray._hit_data))
		{
		case MaterialType::NON_METAL:
			return getNonMetalIntersectionResult(incident_ray, depth);
		case MaterialType::METAL:
			return getMetalIntersectionResult(incident_ray, depth);
		case MaterialType::GLASS:
			return getDielectricIntersectionResult(incident_ray, depth);
		case MaterialType::WATER:
			return getWaterIntersectionResult(incident_ray, depth);
		case MaterialType::EMISSIVE:
			return getEmissiveIntersectionResult(incident_ray, depth);
		default:
			return { 0.0f, 0.0f };
		}
	}
	// If no material found (meaning outside of world), return the sky color.
	else
	{
		return getSkydomeIntersectionResult(incident_ray);
	}

	// If underwater distance is still positive, apply beer's law to water.
}


TraceRecord Renderer::getSkydomeIntersectionResult(Ray& incident_ray) const
{
	return { 1.0f * _world_side_modifier, scene._skydome.GetColor(incident_ray) };
}


TraceRecord Renderer::getNonMetalIntersectionResult(Ray& incident_ray, int depth)
{
	float3 ray_dir_normalized{ normalize(incident_ray.D) };

	float cos_theta{ min(dot(-ray_dir_normalized, incident_ray.normal), 1.0f) };
	float reflect_chance{ getFresnelReflectance(cos_theta) };

	if (_split_on_first_hit && (_max_depth - depth + 1) <= _parallel_depth)
	{
		// Generate secondary ray.
		Ray reflected_ray{ getReflectedRay(incident_ray, incident_ray.normal) };
		const TraceRecord reflected_record{ trace(reflected_ray, depth - 1) };

		// Generate secondary ray.
		Ray scattered_ray{ getScatteredRay(incident_ray, incident_ray.normal) };
		const TraceRecord scattered_record{ trace(scattered_ray, depth - 1) };

		// Get direct illumination. Tint light using absorbance (Beer's Law).
		TintData tint_data{};
		float3 direct_illumination{ scene._light_list.getIllumination(incident_ray, incident_ray.normal, tint_data) };
		float3 beers_absorbance{ tint_data._voxel ? getAbsorption(tint_data._voxel, tint_data._distance) : 1.0f };		

		// Mix results.		
		float inv_reflect_chance{ 1.0f - reflect_chance };

		return {
			// Albedo
			1.0f,
			// Light
			reflected_record.getResult() * reflect_chance
			+ Ray::getAlbedo(incident_ray._hit_data) * (direct_illumination + scattered_record.getResult()) * inv_reflect_chance * beers_absorbance
		};
		
		/*return {
			Ray::getAlbedo(incident_ray._hit_data),
			reflected_record.getResult() * reflect_chance
			+ beers_absorbance * (direct_illumination + scattered_record.getResult()) * inv_reflect_chance
		};*/
	}
	else
	{
		// Determine reflectance using Schlick Approximation (Fresnel).
		if (reflect_chance > RandomFloat())
		{
			// Generate secondary ray.
			Ray reflected_ray{ getReflectedRay(incident_ray, incident_ray.normal) };
			reflected_ray._dielectric_indicator = incident_ray._dielectric_indicator;
			const TraceRecord record{ trace(reflected_ray, depth - 1) };

			// Add underwater distance.
			incident_ray._distance_underwater = reflected_ray._distance_underwater + min(1.0f, reflected_ray.t * reflected_ray.getWaterIndicator());

			// Return new record.
			return { Ray::getAlbedo(incident_ray._hit_data), record.getResult() * reflect_chance };
		}
		else
		{
			// Generate secondary ray.
			Ray scattered_ray{ getScatteredRay(incident_ray, incident_ray.normal) };
			scattered_ray._dielectric_indicator = incident_ray._dielectric_indicator;
			const TraceRecord record{ trace(scattered_ray, depth - 1) };

			// Add underwater distance.
			incident_ray._distance_underwater = scattered_ray._distance_underwater + min(1.0f, scattered_ray.t * scattered_ray.getWaterIndicator());

			// Get direct illumination. Tint light using absorbance (Beer's Law).
			TintData tint_data{};
			float3 direct_illumination{ scene._light_list.getIllumination(incident_ray, incident_ray.normal, tint_data) };
			float3 beers_absorbance{ tint_data._voxel ? getAbsorption(tint_data._voxel, tint_data._distance) : 1.0f };

			// Return new record.
			return { Ray::getAlbedo(incident_ray._hit_data), beers_absorbance * (direct_illumination + record.getResult()) * (1.0f - reflect_chance)};
		}
	}
}


TraceRecord Renderer::getMetalIntersectionResult(Ray& incident_ray, int depth)
{
	// Generate secondary ray.
	Ray reflected_ray{ getReflectedRay(incident_ray, incident_ray.normal) };
	reflected_ray._dielectric_indicator = incident_ray._dielectric_indicator;
	const TraceRecord record{ trace(reflected_ray, depth - 1) };

	// Add underwater distance.
	incident_ray._distance_underwater = reflected_ray._distance_underwater + min(1.0f, reflected_ray.t * reflected_ray.getWaterIndicator());

	// Return new record.
	return { Ray::getAlbedo(incident_ray._hit_data), record.getResult() };
}


TraceRecord Renderer::getDielectricIntersectionResult(Ray& incident_ray, int depth)
{
	bool is_exiting_material{ static_cast<bool>(incident_ray.getGlassIndicator()) };

	// Find the exit if inside material. Must do first since we need the hit data to determine reflectance chance and ior ratio.
	if (is_exiting_material)
	{		
		scene.findMaterialExit(incident_ray, MaterialList::GetType(incident_ray._src_data));

		// Use water as hit data if below water line. This works because findMaterialExit looks for an air voxel or end of the voxel volume.
		// If underwater, both situations will instead result in being underwater.
		static uint underwater_hit_data{ scene._triangles[0]._data };
		incident_ray._hit_data = underwater_hit_data * (incident_ray.IntersectionPoint().y < scene._WATERLINE);
		//incident_ray._hit_data = scene.tri1._triangle._data * (incident_ray.IntersectionPoint().y < scene._WATERLINE);
	}

	// Index of refraction based on the material the ray started in and the new material that was hit.
	float ior_ratio{ scene._material_list[MaterialList::GetIndex(incident_ray._src_data)]._index_of_refraction
		/ scene._material_list[MaterialList::GetIndex(incident_ray._hit_data)]._index_of_refraction };

	// Angle of intersection between the ray and hit material.
	float3 ray_dir_normalized{ normalize(incident_ray.D) };
	float cos_theta = fminf(dot(-ray_dir_normalized, incident_ray.normal), 1.0f);
	
	// USE BOTH RAY RESULTS IN SAME FRAME.
	if (_split_on_first_hit && (_max_depth - depth + 1) <= _parallel_depth)
	{
		float distance_underwater{ 0.0f };

		// Generate reflected ray.
		Ray reflected_ray{ getReflectedRay(incident_ray, incident_ray.normal) };
		reflected_ray._dielectric_indicator = incident_ray._dielectric_indicator;
		const TraceRecord reflected_record{ trace(reflected_ray, depth - 1) };

		distance_underwater += reflected_ray._distance_underwater + min(1.0f, reflected_ray.t * reflected_ray.getWaterIndicator());
		
		// Generate refracted ray.
		Ray refracted_ray{ getRefractedRay(incident_ray, incident_ray.normal, ior_ratio, cos_theta) };
		refracted_ray.setWaterIndicator(refracted_ray.O.y < scene._WATERLINE);
		refracted_ray.setGlassIndicator(!is_exiting_material);
		const TraceRecord refracted_record{ trace(refracted_ray, depth - 1) };

		distance_underwater += refracted_ray._distance_underwater + min(1.0f, refracted_ray.t * refracted_ray.getWaterIndicator());
		
		// Absorbance.
		float3 beers_absorbance{ is_exiting_material ? getAbsorption(incident_ray._src_data, incident_ray.t) : 1.0f };

		// Mix results.
		float reflect_chance{ getFresnelReflectance(cos_theta, ior_ratio) };
		float inv_reflect_chance{ 1.0f - reflect_chance };

		return {
			// Albedo
			Ray::getAlbedo(incident_ray._hit_data) * reflect_chance + inv_reflect_chance,
			// Light
			reflected_record.getResult() * reflect_chance
			+ refracted_record.getResult() * inv_reflect_chance * beers_absorbance
		};
	}

	// USE ONLY 1 RAY RESULT (STOCHASTIC SELECTION) FOR THE FRAME.
	else
	{
		float reflect_chance{ getFresnelReflectance(cos_theta, ior_ratio) };

		// Reflect.
		if (cannotRefract(cos_theta, ior_ratio) || reflect_chance > RandomFloat())
		{
			// Generate secondary ray.
			Ray reflected_ray{ getReflectedRay(incident_ray, incident_ray.normal) };
			reflected_ray._dielectric_indicator = incident_ray._dielectric_indicator;
			const TraceRecord record{ trace(reflected_ray, depth - 1) };

			// Add underwater distance.
			incident_ray._distance_underwater = reflected_ray._distance_underwater + min(1.0f, reflected_ray.t * reflected_ray.getWaterIndicator());

			// Return new record.
			return { 1.0f, record.getResult() };
		}

		// Refract.
		else
		{
			// Generate secondary ray.
			Ray refracted_ray{ getRefractedRay(incident_ray, incident_ray.normal, ior_ratio, cos_theta) };			

			// Leaving.
			if (is_exiting_material)
			{
				// Set indicators.
				refracted_ray.setWaterIndicator(refracted_ray.O.y < scene._WATERLINE);
				refracted_ray.setGlassIndicator(false);
				const TraceRecord record{ trace(refracted_ray, depth - 1) };

				// Add underwater distance.
				incident_ray._distance_underwater = refracted_ray._distance_underwater + min(1.0f, refracted_ray.t * refracted_ray.getWaterIndicator());

				// Exiting applies Beer's Law to determine how much of the material's insides will color the light.
				float3 beers_absorbance{ getAbsorption(incident_ray._src_data, incident_ray.t) };

				// Return new record.			
				return { 1.0f, beers_absorbance * record.getResult() };
			}

			// Entering.
			else
			{
				// Do not count traversal through glass/smoke towards underwater distance even if material is also underwater.
				refracted_ray.setWaterIndicator(false);
				refracted_ray.setGlassIndicator(true);
				const TraceRecord record{ trace(refracted_ray, depth - 1) };

				// Add underwater distance.
				incident_ray._distance_underwater = refracted_ray._distance_underwater + min(1.0f, refracted_ray.t * refracted_ray.getWaterIndicator());

				// Return new record.			
				return { 1.0f, record.getResult() };
			}
		}
	}
}


// Special dielectric resolution for hitting water - which is only on triangle primitives.
TraceRecord Renderer::getWaterIntersectionResult(Ray& incident_ray, int depth)
{	
	// Index of refraction based on the material the ray started in and the new material that was hit.
	float ior_ratio{ scene._material_list[MaterialList::GetIndex(incident_ray._src_data)]._index_of_refraction
		/ scene._material_list[MaterialList::GetIndex(incident_ray._hit_data)]._index_of_refraction };

	// Angle of intersection between the ray and hit material.
	float3 ray_dir_normalized{ normalize(incident_ray.D) };
	float cos_theta = fminf(dot(-ray_dir_normalized, incident_ray.normal), 1.0f);

	// Chance of Fresnel refleaction.
	float reflect_chance{ getFresnelReflectance(cos_theta, ior_ratio) };

	// USE BOTH RAY RESULTS IN SAME FRAME.
#if 1
	if (_split_on_first_hit && (_max_depth - depth + 1) <= _parallel_depth) 
	{
		float distance_underwater{ 0.0f };

		// Generate reflected ray.
		Ray reflected_ray{ getReflectedRay(incident_ray, incident_ray.normal) };
		reflected_ray._dielectric_indicator = incident_ray._dielectric_indicator;
		const TraceRecord reflected_record{ trace(reflected_ray, depth - 1) };

		distance_underwater = reflected_ray._distance_underwater + min(1.0f, reflected_ray.t * reflected_ray.getWaterIndicator());

		// Generate refracted ray.
		Ray refracted_ray{ getRefractedRay(incident_ray, incident_ray.normal, ior_ratio, cos_theta) };
		refracted_ray.setWaterIndicator(true);
		const TraceRecord refracted_record{ trace(refracted_ray, depth - 1) };

		distance_underwater += refracted_ray._distance_underwater + min(1.0f, refracted_ray.t * refracted_ray.getWaterIndicator());

		// Absorbance.
		float3 beers_absorbance{ incident_ray.getWaterIndicator() == 0 ? getAbsorption(incident_ray._hit_data, distance_underwater) : 1.0f };

		// Mix results.
		float inv_reflect_chance{ 1.0f - reflect_chance };

		return {
			// Albedo
			Ray::getAlbedo(incident_ray._hit_data) * reflect_chance + inv_reflect_chance,
			// Light
			reflected_record.getResult() * reflect_chance
			+ refracted_record.getResult() * inv_reflect_chance * beers_absorbance
		};
	}
#endif
	// Reflect.
	if (cannotRefract(cos_theta, ior_ratio) || reflect_chance > RandomFloat())
	{
		// Generate secondary ray.
		Ray reflected_ray{ getReflectedRay(incident_ray, incident_ray.normal) };
		reflected_ray._dielectric_indicator = incident_ray._dielectric_indicator;
		const TraceRecord record{ trace(reflected_ray, depth - 1) };

		// Add underwater distance.
		incident_ray._distance_underwater = reflected_ray._distance_underwater + min(1.0f, reflected_ray.t * reflected_ray.getWaterIndicator());

		// Return new record.
		return { 1.0f, record.getResult() };
	}

	// Enter/Exit material via refraction.
	else
	{
		// Generate secondary ray.
		Ray refracted_ray{ getRefractedRay(incident_ray, incident_ray.normal, ior_ratio, cos_theta) };

		// Leaving water.
		if (incident_ray.getWaterIndicator())
		{
			refracted_ray.setWaterIndicator(false);
			const TraceRecord record{ trace(refracted_ray, depth - 1) };

			// Add underwater distance.
			incident_ray._distance_underwater = refracted_ray._distance_underwater + min(1.0f, refracted_ray.t * refracted_ray.getWaterIndicator());
							
			// Return new record.			
			return { 1.0f, record.getResult() };
		}

		// Entering water.
		else
		{
			refracted_ray.setWaterIndicator(true);
			const TraceRecord record{ trace(refracted_ray, depth - 1) };

			// Add underwater distance.
			incident_ray._distance_underwater = refracted_ray._distance_underwater + min(1.0f, refracted_ray.t * refracted_ray.getWaterIndicator());

			// Apply Beer's Law. Use distance underwater to determine absorption amount.
			float3 beers_absorbance{ getAbsorption(incident_ray._hit_data, incident_ray._distance_underwater) };
			
			// Reset count. If any distance remains once primary ray tries to resolve, it will apply beer's law to the light again.
			incident_ray._distance_underwater = 0.0f;

			// Return new record.			
			return { 1.0f, beers_absorbance * record.getResult() };
		}
	}
}


TraceRecord Renderer::getEmissiveIntersectionResult(Ray& incident_ray, int)
{
	return { Ray::getAlbedo(incident_ray._hit_data), scene._material_list[MaterialList::GetIndex(incident_ray._hit_data)]._emissive_intensity };
}


// Beer-Lambert law.
float3 Renderer::getAbsorption(const uint dielectric_data, const float distance_traveled)
{
	const float3 flipped_color{ 1.0f - Ray::getAlbedo(dielectric_data) };

	float intensity(scene._material_list[MaterialList::GetIndex(dielectric_data)]._absorption_intensity);
	
	// Combining 'e' and 'c' terms into a single "density" value (stored as intensity in the material).
	// [Credit] https://www.flipcode.com/archives/Raytracing_Topics_Techniques-Part_3_Refractions_and_Beers_Law.shtml
	const float3 exponent{ -distance_traveled * intensity * flipped_color };

	return { expf(exponent.x), expf(exponent.y), expf(exponent.z) };
}


bool Renderer::cannotRefract(const float cos_theta, const float ior_ratio)
{
	// Determine if the ray can refract into the material
	// [Credit] Ray Tracing in One Weekend (P Shirley).
	const float sin_theta = sqrtf(1.0f - cos_theta * cos_theta);

	return ior_ratio * sin_theta > 1.0f;
}


// Use Schlick's approximation for reflectance using index of refractions (dielectrics).
float Renderer::getFresnelReflectance(const float cos_theta, const float ior_ratio)
{	
	// [Credit] Ray Tracing in One Weekend (P Shirley).
	float r0 = (1.0f - ior_ratio) / (1.0f + ior_ratio);
	r0 = r0 * r0;
	return r0 + (1.0f - r0) * pow((1.0f - cos_theta), 5.0f);
}


// Use Schlick's approximation for reflectance using a given Fresnel coefficient value (non-metals).
float Renderer::getFresnelReflectance(const float cos_theta)
{
	// [Credit] Ray Tracing in One Weekend (P Shirley).
	constexpr static float nonmetal_Fresnel_coefficient{ 0.04f };
	return nonmetal_Fresnel_coefficient + (1.0f - nonmetal_Fresnel_coefficient) * pow((1.0f - cos_theta), 5.0f);
}


Ray Renderer::getScatteredRay(const Ray& incident_ray, const float3& ray_normal)
{
	float3 scatter_direction{ weightedRandomOnHemisphere(ray_normal) };
	
	return { incident_ray.IntersectionPoint(), scatter_direction, incident_ray._hit_data, true };
}


Ray Renderer::getReflectedRay(const Ray& incident_ray, const float3& ray_normal)
{
	// Get perfect reflection.	
	float3 ray_dir_normalized{ normalize(incident_ray.D) };
	float3 reflected_direction{ reflect(ray_dir_normalized, ray_normal) };

	// Add jitter.
	const float roughness{ scene._material_list[MaterialList::GetIndex(incident_ray._hit_data)]._roughness };
	if (roughness > 0.0f)
	{
		const float3 jitter{ randomInUnitSphere() * roughness };
		reflected_direction += jitter;
	}
		
	return { incident_ray.IntersectionPoint(), reflected_direction, incident_ray._src_data, true };
}


Ray Renderer::getRefractedRay(const Ray& incident_ray, const float3& ray_normal, const float ior_ratio, const float cos_theta)
{
	float3 ray_dir_normalized{ normalize(incident_ray.D) };
	const float3 ray_out_perp = ior_ratio * (ray_dir_normalized + cos_theta * ray_normal);
	const float3 ray_out_par = -sqrtf(fabsf(1.0f - sqrLength(ray_out_perp))) * ray_normal;
	const float3 refracted_ray_direction = ray_out_perp + ray_out_par;	

	return { incident_ray.IntersectionPoint(), refracted_ray_direction, incident_ray._hit_data, true };
}


float3 Renderer::uncharted2ToneMapPartial(const float3 color)
{
	static constexpr float A{ 0.15f };
	static constexpr float B{ 0.50f };
	static constexpr float C{ 0.10f };
	static constexpr float D{ 0.20f };
	static constexpr float E{ 0.02f };
	static constexpr float F{ 0.30f };
	
	static const float3 BC{ B * C };
	static const float3 DE{ D * E };
	static const float3 DF{ D * F };
	static const float3 EF{ E / F };

	float3 AX{ color * A };

	return ((color * (AX + BC) + DE) / (color * (AX + B) + DF)) - EF;
}


float3 Renderer::uncharted2Filmic(const float3 color)
{
	static float exposure_bias{ 5.0f };
	float3 current{ uncharted2ToneMapPartial(color * exposure_bias) };

	static float3 W{ 11.2f };
	static float3 white_scale{ float3{1.0f} / uncharted2ToneMapPartial(W) };

	return current * white_scale;
}


float3 Renderer::tonemap(const float3& color)
{
	return { uncharted2Filmic(color) };
}


void Renderer::showAudioCard(const float delta_time)
{
	if (scene._audio_card.update(delta_time))
	{
		scene._audio_card.copyTrimmedCardTo(screen);
	}
}


// -----------------------------------------------------------
// Update user interface (imgui)
// -----------------------------------------------------------
void Renderer::UI()
{
	if (!ImGui::Begin("Debug Dreamer"))
	{
		ImGui::End();
		return;
	}

	// Begin ImGui data.
	

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("Use together: ");
	ImGui::SameLine();
	if (ImGui::Checkbox("Use Reprojection", &_use_reprojection))
	{
		_use_accumulator = false;
		resetAccumulator();
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Use Denoising", &_use_denoiser)) 	
	{
		_use_accumulator = false;
		resetAccumulator();
	}	

	ImGui::Text("Use together: ");
	ImGui::SameLine();
	if (ImGui::Checkbox("Use Accumulator", &_use_accumulator))
	{
		_use_reprojection = false;
		_use_denoiser = false;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Use Anti-aliasing", &_use_antialiasing);


	ImGui::BeginTabBar("##TabBar");

	if (ImGui::BeginTabItem("Camera"))
	{
		ImGui::Text("Position: %.2f, %.2f, %.2f", _camera._position.x, _camera._position.y, _camera._position.z);
		ImGui::Text("   Ahead: %.4f, %.4f, %.4f", _camera._ahead.x, _camera._ahead.y, _camera._ahead.z);
		
		ImGui::EndTabItem();
	}
			
	if (ImGui::BeginTabItem("Rays"))
	{
		ImGui::SliderInt("Max Depth", &_max_depth, 1, 20);
		
		ImGui::Spacing();

		static int parallel_trace{ _split_on_first_hit ? 1 : 0 };
		ImGui::Text("Trace both secondary rays: ");
		if (ImGui::RadioButton("Off", &parallel_trace, 0))
		{
			_split_on_first_hit = false;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("On", &parallel_trace, 1))
		{
			_split_on_first_hit = true;
		}
		ImGui::SliderInt("Split count", &_parallel_depth, 0, 10);

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Materials"))
	{
		int material_count = 0;

		std::string prefix{ "##" };

		for (size_t i = 0; i < scene._material_list._materials.size(); ++i)
		{
			std::string header{ "Material " + std::to_string(++material_count) };

			if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_None))
			{	
				ImGui::PushItemWidth(60);
				
				Material& material{ scene._material_list._materials[i] };

				ImGui::SliderFloat((prefix + header + "Roughness").c_str(), &material._roughness, 0.0f, 1.0f);				
				ImGui::SameLine();
				ImGui::Text("Roughness");

				ImGui::SliderFloat((prefix + header + "IOR").c_str(), &material._index_of_refraction, 0.0f, 5.0f);
				ImGui::SameLine();
				ImGui::Text("Index of refraction");

				ImGui::SliderFloat((prefix + header + "INT").c_str(), &material._absorption_intensity, 0.0f, 50.0f);
				ImGui::SameLine();
				ImGui::Text("Intensity");

				ImGui::PopItemWidth();
			}
		}

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Lights"))
	{
		ImGui::Text("Add Light: "); 
		ImGui::SameLine();
		if (ImGui::SmallButton("Directional"))
		{
			scene._light_list.addDirectionalLight({ 1.0f }, 1.0f, { 0.0f, -1.0f, 0.0f });			
		}
		if (ImGui::SmallButton("Point"))
		{
			scene._light_list.addPointLight({ 1.0f }, 1.0f, { 0.5f, 1.0f, 0.5f });
		}
		if (ImGui::SmallButton("Spot"))
		{
			scene._light_list.addSpotlight({ 1.0f }, 1.0f, { 0.0f, -1.0f, 0.0f }, { 0.5f, 1.0f, 0.5f });		
		}
		if (ImGui::SmallButton("Area"))
		{
			scene._light_list.addAreaLightRectangle({ 1.0f }, 1.0f, { 0.0f, -1.0f, 0.0f }, { 0.5f, 1.0f, 0.5f }, { 1.0f, 1.0f });
		}

		std::string prefix{ "##" };
		std::string counter_str{ "0" };

		bool light_was_changed{ false };
		int light_id_to_remove{ -1 };

		ImGui::PushItemWidth(200);

		for (int i = 0; i < scene._light_list._lights.size(); ++i)
		{
			counter_str = std::to_string(i);
			Light& light{ scene._light_list._lights[i] };

			// Every light has these properties.
			std::string button_title{ "Remove (" + counter_str + ")" };
			if (ImGui::SmallButton(button_title.c_str()))
			{
				light_id_to_remove = light._id;
			}

			if (ImGui::ColorEdit3((prefix + "col" + counter_str).c_str(), light._color.cell))
			{
				light_was_changed = true;
			}
			ImGui::SameLine();
			ImGui::Text("Color");

			if (ImGui::SliderFloat((prefix + "int" + counter_str).c_str(), &light._intensity, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_None))
			{
				light_was_changed = true;
			}
			ImGui::SameLine();
			ImGui::Text("Intensity");

			// Show properties unique to the light.
			switch (light._type)
			{
			case LightType::DIRECTIONAL:
			{
				if (ImGui::SliderFloat3((prefix + "dir" + counter_str).c_str(), light._direction_to_world.cell, -1.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Direction");

				break;
			}
			case LightType::POINT:
			{
				if (ImGui::SliderFloat3((prefix + "pos" + counter_str).c_str(), light._position.cell, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Position");

				break;
			}
			case LightType::SPOT:
			{
				if (ImGui::SliderFloat3((prefix + "pos" + counter_str).c_str(), light._position.cell, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Position");

				if (ImGui::SliderFloat3((prefix + "dir" + counter_str).c_str(), light._direction_to_world.cell, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Direction");

				if (ImGui::SliderFloat((prefix + "ptheta" + counter_str).c_str(), &light._cutoff_cos_theta, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
				}
				ImGui::SameLine();
				if (ImGui::SliderFloat((prefix + "pfalloff" + counter_str).c_str(), &light._falloff_factor, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Cutoff & Falloff");

				break;
			}
			case LightType::AREA_RECT:
			{
				bool orientation_was_changed{ false };

				if (ImGui::SliderFloat3((prefix + "pos" + counter_str).c_str(), light._position.cell, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
					orientation_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Position");

				if (ImGui::SliderFloat3((prefix + "dir" + counter_str).c_str(), light._direction_to_world.cell, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
					orientation_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Direction");

				if (ImGui::SliderFloat2((prefix + "size" + counter_str).c_str(), light._size.cell, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
					orientation_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Direction");

				if (orientation_was_changed)
				{
					light.calculateAreaRectOrientation();
				}

				break;
			}
			case LightType::AREA_SPHERE:
			{
				if (ImGui::SliderFloat3((prefix + "pos" + counter_str).c_str(), light._position.cell, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Position");

				if (ImGui::SliderFloat((prefix + "size" + counter_str).c_str(), &light._radius, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_None))
				{
					light_was_changed = true;
				}
				ImGui::SameLine();
				ImGui::Text("Direction");

				break;
			}
			default:
				break;
			}

			if (i + 1 != scene._light_list._lights.size())
			{
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
			}
		}

		ImGui::PopItemWidth();

		if (light_id_to_remove != -1)
		{
			// 1st go from end to removed light and decrement ids by 1.
			for (size_t i = scene._light_list._lights.size(); i-- > 0; )
			{
				if (scene._light_list._lights[i]._id != light_id_to_remove)
				{
					--scene._light_list._lights[i]._id;
				}
				else
				{
					break;
				}
			}

			// 2nd go from start to removed light via iterator to remove.
			for(auto i = scene._light_list._lights.begin(); i != scene._light_list._lights.end(); ++i)
			{
				if ((*i)._id == light_id_to_remove)
				{
					scene._light_list._lights.erase(i);
					--scene._light_list._max_range;
					break;
				}
			}
		}

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Player"))
	{
		Player& player{ scene._player };
		
		ImGui::Text("Position: %.2f, %.2f, %.2f", player._avatar._position.x, player._avatar._position.y, player._avatar._position.z);
		ImGui::Text("Ahead: %.2f, %.2f, %.2f", player._ahead.x, player._ahead.y, player._ahead.z);
	}

	ImGui::EndTabBar();

	ImGui::End();
}


// -----------------------------------------------------------
// User input
// -----------------------------------------------------------
void Renderer::MouseUp(int) {}


void Renderer::MouseDown(int) {}


// -----------------------------------------------------------
// User wants to close down
// -----------------------------------------------------------
void Renderer::Shutdown()
{
	FREE64(_albedo_buffer);
	FREE64(_pixel_new_buffer);
	FREE64(_pixel_reprojected_buffer);
	FREE64(_pixel_history_buffer);

	delete[] _ray_buffer;
	delete screen;
}