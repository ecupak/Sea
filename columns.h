#pragma once

class Columns
{
public:
	Columns() = default;

	static void build(Scene& scene, Cube& world)
	{
		// Materials.
		uint ground{ scene._material_list.AddNonMetal() };
		uint glowing{ scene._material_list.AddEmissive(2.0f) };

		// Reused vars.
		uint color{ 0 };
		uint material{ 0 };

		// Map properties.
		int length = 100;
		int gap = 10;

		// Ground.
		int ground_level = 0;

		if (true)
		{
			color = 0xA2A2A2;
			material = ground;

			for (int x = 0; x < gap; ++x)
			{
				for (int z = 0; z < length; ++z)
				{
					world.set(x, ground_level, z, material, color);
				}
			}
		}

		// Columns.
		int max_height = 6;
		for (int r = 0; r < 2; ++r)
		{
			int lamp_offset{ r == 0 ? 1 : -1 };
			int x_pos{ r * (gap - 1) };

			for (int z = 0; z < length; ++z)
			{
				if (z % 2 == 0)
				{
					world.set(x_pos, max_height, z, ground, color);
				}
				else
				{
					for (int y = 0; y < max_height; ++y)
					{
						int y_pos = y + 1;

						world.set(x_pos, y_pos, z, ground, color);

						if (y == max_height - 3 && (z % 7) == 0)
						{
							world.set(x_pos + lamp_offset, y_pos, z, glowing, 0xFF0000);
						}
					}
				}
			}
		}
	}
};