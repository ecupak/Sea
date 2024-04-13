#pragma once

namespace Tmpl8 
{
	class Scene
	{
	public:
		enum class Stage
		{
			INTRO,
			WORLD,
		};

		Scene(Camera& camera);
		~Scene();

		void generateScene(const Stage stage);
		void update(const float delta_time, const KeyboardManager& km);

		void refitAS();

		bool findNearest(Ray& ray) const;
		void findNearestToPlayer(Ray& player_ray, const int source_id) const;
		void findMaterialExit(Ray& ray, const uint material_type) const;
		bool isOccluded(Ray& ray) const;
		bool isOccluded(Ray& ray, TintData& tint_datang) const;				
		void eraseVoxels(Ray& ray);
		void restoreVoxels();
		
		Stage _stage{ Stage::INTRO };

		std::vector<Sphere> _spheres;
		SphereBVH _sphere_bvh{ _spheres };

		// Lights.
		LightList _light_list{ this };

		// Audio.
		CardManager _audio_card{ "assets/icons/musical_score.png" };
		AudioManager _audio_manager{ _audio_card };		

		// Intro.
		Surface _title_image{ "assets/flipped_title.png" };
		CubeBVH* _title_cubes{ nullptr };
		
		// The sea.
		static constexpr float _WATERLINE{ 0.5f };
		std::vector<Triangle> _triangles;
		TriBVH _triangle_bvh{ _triangles };

		// The islands.
		static constexpr int _ISLAND_COUNT{ 11 };
		std::array<Island, _ISLAND_COUNT> _islands;

		// The walls.
		static constexpr int _WALL_COUNT{ 11 };
		std::array<Wall, _WALL_COUNT> _walls;

		// The player.
		Player _player;

		// TLAS / BLAS's
		std::vector<BVH*> _bvh_list;
		TLAS _tlas{ _bvh_list };
		
		// Cubes that need to be restored.
		Cube* modified_cube{ nullptr };

		MaterialList _material_list;
		SkyDome _skydome{ "assets/skydomes/cloudy_sky.hdr" };

	private:
		void updateIntro(const float delta_time, const KeyboardManager& km);
		void updateWorld(const float delta_time, const KeyboardManager& km);
		
		void generateIntro(uint non_metal_id, uint emissive_id, uint glass_id);
		void generateWorld(uint non_metal_id, uint emissive_id, uint glass_id);


		Island::Data _island_data[_ISLAND_COUNT]
		{
			// This island has a charge stone. It is purple (0xB100CD).
			// Charge stone colors match the island they need to go to.
			{	Island::Location::ABOVE,	0x00FF00,
				{ 3.55f, 1.05f },			{ 0, 1 },
				Stone::Type::CHARGE,		0xB100CD,
				-1,							{ -1, 1 }
			},

			// This island will be lit by the purple stone.
			// For the island color, use the index of the island with the stone.
			// All islands with empty stones will do this.
			{	Island::Location::BELOW,	0, 
				{ 3.0f, 1.65f },			{ 1, 0 },
				Stone::Type::EMPTY,			0x51414F,
				-1,							{ -1, 2 }
			},

			// This island will react once the previous island is lit.
			// It will observe that island and be notified when that happens.
			// Location::SINK means it will be inaccessible until triggered to sink.
			// Its starting midpoint position will be above the water line.			
			// This island also has an upgrade stone. Provide its value.
			{	Island::Location::SINK,		0x11AA40, 
				{ 3.9f, 1.6f },				{ -1, 0 },
				Stone::Type::UPGRADE,		0x51414F,
				1,							{ 1, -1 }
			},

			// MAIN ISLANDS

			{	Island::Location::ABOVE,	0xFF0000,
				{ 6.5f, 4.5f },				{ 0, 1 },
				Stone::Type::CHARGE,		0x12EE55,
				-1,							{ -1, -1 }
			}, // 3

			{	Island::Location::BELOW,	3,
				{ 2.5f, 7.5f },				{ 0, -1 },
				Stone::Type::EMPTY,			0xCCCCCC,
				-1,							{ -1, -1 }
			}, // 4

			{	Island::Location::BELOW,	0xB100CD,
				{ 3.5f, 6.5f },				{ 0, 1 },
				Stone::Type::UPGRADE,		0xFF0000,
				1,							{ -1, -1 }
			}, // 5

			{	Island::Location::SINK,		0xB100CD,
				{ 2.5f, 5.5f },				{ 1, 0 },
				Stone::Type::UPGRADE,		0xFF0000,
				1,							{ 5, -1 }
			}, // 6

			{	Island::Location::SINK,		0xAABB44,
				{ 4.5f, 5.5f },				{ 0, -1 },
				Stone::Type::CHARGE,		0x00DDDD,
				-1,							{ 4, -1 }
			}, // 7

			{	Island::Location::BELOW,	7,
				{ 1.5f, 7.5f },				{ 0, -1 },
				Stone::Type::EMPTY,			0xCCCCCC,
				-1,							{ -1, -1 }
			}, // 8

			{	Island::Location::SINK,		0xFF6644,
				{ 6.0f, 7.0f },				{ -1, 0 },
				Stone::Type::CHARGE,		0xFFA500,
				-1,							{ 8, -1 }
			}, // 9

			{	Island::Location::BELOW,	9,
				{ 0.5f, 4.5f },				{ -1, 0 },
				Stone::Type::EMPTY,			0xCCCCCC,
				-1,							{ -1, -1 }
			}, // 10
			
		};

		float mid{ _WATERLINE };


		float3 _a_lower_bounds{ 2.0f, mid, 0.0f };
		float3 _a_upper_bounds{ 5.0f, mid, 3.0f };
		
		float3 _c_lower_bounds{ 0.0f, mid, 4.0f };
		float3 _c_upper_bounds{ 7.0f, mid, 8.0f };

		float _lower_x_midpoint{ _a_lower_bounds.x + 0.5f * (_a_upper_bounds.x - _a_lower_bounds.x) };
		float2 _gate_x_points{
			_lower_x_midpoint - 21.0f * static_cast<float>(VOXELSIZE),
			_lower_x_midpoint + 21.0f * static_cast<float>(VOXELSIZE)
		};
		float _main_z_midopint{ _c_lower_bounds.z + 0.5f * (_c_upper_bounds.z - _c_lower_bounds.z) };

		uint _wall_height{ 40 };
		uint _wall_color{ 0xA2A2A2 };
		uint _lower_width{ static_cast<uint>((_a_upper_bounds.x - _a_lower_bounds.x) * WORLDSIZE) };
		uint2 _main_size{ 
			static_cast<uint>(7.0f * WORLDSIZE),
			static_cast<uint>(4.0f * WORLDSIZE)
		};
		uint _side_width{ static_cast<uint>((_c_upper_bounds.z - _c_lower_bounds.z) * WORLDSIZE) };
		
		Wall::Data _wall_data[_WALL_COUNT]
		{
			// TUTORIAL

			// Bottom wall.
			{
				Wall::Type::BOUNDARY,	{ _lower_width, _wall_height, 1 },
				0.0f,					{ _lower_x_midpoint, _WATERLINE, _a_lower_bounds.z },
				-1,	-1
			},

			// Left wall.
			{
				Wall::Type::BOUNDARY,	{ _lower_width, _wall_height, 1 },
				PI * 0.5f,				{ _a_lower_bounds.x, _WATERLINE, 0.5f * (_a_upper_bounds.z - _a_lower_bounds.z) },
				-1, -1
			},

			// Right wall.
			{
				Wall::Type::BOUNDARY,	{ _lower_width, _wall_height, 1 },
				PI * 0.5f,				{ _a_upper_bounds.x, _WATERLINE, 0.5f * (_a_upper_bounds.z - _a_lower_bounds.z) },
				-1, -1
			},

			// Top wall.
			{
				Wall::Type::GATE,		{ _lower_width, _wall_height, 1 },
				0.0f,					{ _lower_x_midpoint, _WATERLINE, _a_upper_bounds.z },
				2, 0
			},

			// HALL

			// Left wall
			{
				Wall::Type::BOUNDARY,	{ WORLDSIZE, _wall_height, 1 },
				PI * 0.5f,				{ _gate_x_points.x, _WATERLINE, 3.5f },
				-1, -1
			},

			// Right wall
			{
				Wall::Type::BOUNDARY,	{ WORLDSIZE, _wall_height, 1 },
				PI * 0.5f,				{ _gate_x_points.y, _WATERLINE, 3.5f },
				-1, -1
			},

			// MAIN ZONE

			// Bottom wall
			{
				Wall::Type::GATE,		{ _main_size.x, _wall_height, 1 },
				0.0f,					{ _lower_x_midpoint, _WATERLINE, _c_lower_bounds.z },
				2, 1
			},

			// Top wall
			{
				Wall::Type::BOUNDARY,	{ _main_size.x, _wall_height, 1 },
				0.0f,					{ _lower_x_midpoint, _WATERLINE, _c_upper_bounds.z },
				-1, -1
			},

			// Left wall
			{
				Wall::Type::BOUNDARY,	{ _main_size.y, _wall_height, 1 },
				PI * 0.5f,				{ 0.0f, _WATERLINE, _main_z_midopint },
				-1, -1
			},

			// Right wall
			{
				Wall::Type::BOUNDARY,	{ _main_size.y, _wall_height, 1 },
				PI * 0.5f,				{ 7.0f, _WATERLINE, _main_z_midopint },
				-1, -1
			},

			// Dividing wall
			{
				Wall::Type::GATE,		{ _main_size.y, _wall_height, 1 },
				PI * 0.5f,				{ 2.0f, _WATERLINE, _main_z_midopint },
				6, 2
			},
		};

		int2 _gate_openings[3]
		{
			make_int2(static_cast<int>(_lower_width * 0.5f), 20),
			make_int2(static_cast<int>(_main_size.x * 0.5f), 20),
			make_int2(static_cast<int>(_main_size.y * 0.5f), 10),
		};
	};
}