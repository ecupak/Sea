#pragma once

namespace Tmpl8
{
	class TraceRecord
	{
	public:
		TraceRecord(float3 albedo, float3 light)
			:_albedo{ albedo }
			, _light{ light }
		{	}

		float3 _albedo{ 1.0f };				// 12 bytes
		float3 _light{ 1.0f };				// 12 bytes
		
		inline float3 getResult() const
		{
			return _albedo * _light;
		}
	};


	struct PreviousCamera
	{
		float3 _position{ 1.0f };
	};


	class Renderer : public TheApp
	{
	public:
		~Renderer() override;

		// Game flow methods
		void Init();	
		void Tick( float deltaTime );
				
		// Input handling
		void MouseUp(int button);
		void MouseDown(int button);
		void MouseMove(int x, int y) { _km.mouseMovedTo(make_int2(x, y)); }
		void MouseWheel(float) { /* implement if you want to handle the mouse wheel */ }
		void KeyUp(int key) { _km.keyReleased(key); }
		void KeyDown(int key) { _km.keyPressed(key); }

		// Miscellaneous
		void UI();
		void Shutdown();

		// Main methods.
		void shootErasureRays(float2 coordinates[]);
		void shootPrimaryRays();		
		void applyReprojection();
		void applyDenoising();
		void resetAccumulator();

		// Ray interaction logic.
		TraceRecord trace(Ray& ray, int depth);
		TraceRecord getSkydomeIntersectionResult(Ray& incident_ray) const;
		TraceRecord getNonMetalIntersectionResult(Ray& incident_ray, int depth);
		TraceRecord getMetalIntersectionResult(Ray& incident_ray, int depth);
		TraceRecord getDielectricIntersectionResult(Ray& incident_ray, int depth);
		TraceRecord getWaterIntersectionResult(Ray& incident_ray, int depth);
		TraceRecord getEmissiveIntersectionResult(Ray& incident_ray, int depth);

		// Ray interaction logic helpers.
		float3 getAbsorption(const uint dielectric_data, const float distance_traveled);
		static bool cannotRefract(const float cos_theta, const float ior_ratio);
		static float getFresnelReflectance(const float cos_theta, const float ior_ratio);
		static float getFresnelReflectance(const float cos_theta);

		// Secondary ray creation.
		static Ray getScatteredRay(const Ray& incident_ray, const float3& ray_normal);
		Ray getReflectedRay(const Ray& incident_ray, const float3& ray_normal);
		static Ray getRefractedRay(const Ray& incident_ray, const float3& ray_normal, const float ior_ratio, const float cos_theta);
		
		// Tonemapping
		static float3 tonemap(const float3& color);
		static float3 uncharted2Filmic(const float3 color);
		static float3 uncharted2ToneMapPartial(const float3 color);

		// Reprojection
		bool findPreviousPixelPosition(const uint current_pixel_index, float2& previous_pixel) const;
		float3 getHistorySample(const float2& history_pixel_position) const;
		void applyColorClamping(float3& color_to_clamp, float3 reference_color, int2 reference_color_position) const;
		static float3 RGB_to_YCoCg(float3 rgb);
		static float3 YCoCg_to_RGB(float3 rgb);

		// Denoising
		void applySeperableBilinearFiltering(const int current_pixel_index, const int2 current_pixel_origin, const int range, const int2 axis, float3* read_from, float3* write_to);
		float getCommonPropertiesWeighting(const Ray& main_ray, const int2& other_position);

		// Audio card.
		void showAudioCard(const float delta_time);

		// Input trackers.
		KeyboardManager _km{};

		// Reprojection / Denoising workflow helpers.
		Scene scene{ _camera };
		Camera _camera;
		Camera _retired_camera{};
		
		float d0{ 0.0f };
		
		Ray _manual_focus_ray;
		HaltonSequencer _halton_sequencer{};		
		
		float d1{ 0.0f };

		float2 _screenf{ static_cast<float>(SCRWIDTH), static_cast<float>(SCRHEIGHT) };		
		float2 _midscreenf{ static_cast<float>(SCRWIDTH >> 1), static_cast<float>(SCRHEIGHT >> 1) };
		float2 _subpixel_positions[256];
		int2 _focal_point{ SCRWIDTH >> 1, SCRHEIGHT >> 1 };

		Ray*	_ray_buffer{ nullptr };
		float3* _albedo_buffer{ nullptr };
		float3* _pixel_reprojected_buffer{ nullptr };
		float3* _pixel_history_buffer{ nullptr };
		float3* _accumulator{ nullptr };
		union
		{
			float3* _pixel_new_buffer{ nullptr };
			float3* _pixel_denoise_buffer;			
		};
		
		// Halton samples used for subpixel locations.
		static constexpr int _HALTON_SAMPLE_SIZE{ 256 };
		int _offset_index{ 0 };	

		// Precompute the float of the worldsize.
		float _world_float{ static_cast<float>(WORLDSIZE) };		

		bool _split_on_first_hit{ true };
		int _parallel_depth{ 1 };
		float _sigma{0.2f};

		// Properties
		int _max_depth{ 6 };		
		float _world_side_modifier{ 1.0f };
		bool _use_reprojection{ true };
		bool _use_denoiser{ true };
		bool _use_accumulator{ false };
		bool _use_antialiasing{ false };
		float _frame_count{ 0.0f };
	};

} // namespace Tmpl8
