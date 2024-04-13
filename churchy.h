#pragma once


class Churchy
{
public:
	Churchy() = default;

	static void build(Scene& scene, Cube& world)
	{
		// Materials.
		uint ground{ scene._material_list.AddNonMetal() };
		uint stained_glass{ scene._material_list.AddGlass(1.477f, 2.0f) };
		
		// Reused vars.
		uint color{ 0 };
		uint material{ 0 };

		// Setup PNG.
		Surface png{ "assets/flipped_glass.png" };
		
		// Map properties.
		int map = png.width * 2;


		// Ground.
		int ground_level = 0;

		if (true)
		{
			color = 0xA2A2A2;
			material = ground;

			for (int x = 0; x < map; ++x)
			{
				for (int z = 0; z < map; ++z)
				{
					world.set(x, ground_level, z, material, color);
				}
			}
		}

		// Stained glass.
		if (true)
		{
			material = stained_glass;

			// Is along X axis. Faces on Z axis.
			int x_offset = (map - png.width) / 2;
			int z = map / 2;

			for (int y = 0; y < png.height; ++y)
			{
				for (int x = 0; x < png.width; ++x)
				{
					color = png.pixels[x + y * png.width];

					world.set(x + x_offset, y + ground_level, z, material, color);
				}
			}
		}
	}
};