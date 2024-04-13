#pragma once


class City
{
public:
	City() = default;

	static void build(Scene& scene, Cube& world)
	{
		// Materials.
		uint block{ scene._material_list.AddNonMetal() };
		uint mirror{ scene._material_list.AddMetal() };
		uint panel{ scene._material_list.AddEmissive(10.0f) };

		// Reused vars.
		uint color{ 0 };
		uint material{ 0 };
		
		// Map properties.
		int cell = 12;
		int side = 5;
		int scale = 1;
		int map = cell * side;

		bool set_ground{ true };
		bool split_into_fifths{ true };
		bool set_city{ true };

		// Ground.
		int ground_level = 0;
		if (set_ground)
		{
			// Make center line on each axis normal ground, rest is mirror.
			if (split_into_fifths)
			{
				int fifth = map / 5;

				for (int x = 0; x < map; ++x)
				{		
					bool x_centerline{ (x >= fifth * 2 && x < fifth * 3) };

					for (int z = 0; z < map; ++z)
					{
						bool z_centerline{ (z >= fifth * 3 && z < fifth * 4) };

						if (x_centerline || z_centerline)
						{
							material = mirror;
							color = 0xFFFFFF;
						}
						else
						{
							material = block;
							color = 0xA2A2A2;
						}

						world.set(x, ground_level, z, material, color);
					}
				}
			}
			
			// Or make it all normal ground.
			else
			{
				color = 0xA2A2A2;
				material = block;

				for (int x = 0; x < map; ++x)
				{
					for (int z = 0; z < map; ++z)
					{
						world.set(x, ground_level, z, material, color);
					}
				}
			}
		}

		// City.
		if (set_city)
		{
			int available_space = cell - scale;
			float chance_of_panel{ 0.2f };
			const int max_floors{ 3 };
			float chance_of_floor[max_floors] { 0.9f, 0.5f, 0.3f };

			for (int x = 0; x < map - cell; x += cell)
			{				
				for (int z = 0; z < map - cell; z += cell)
				{
					// Get random placement of building.
					int x_origin = static_cast<int>(x + (RandomFloat() * available_space));
					int z_origin = static_cast<int>(z + (RandomFloat() * available_space));

					// Reset y.
					int y = ground_level + 1;

					for (int f = 0; f < max_floors; ++f)
					{
						// Add a floor.
						if (chance_of_floor[f] > RandomFloat())
						{
							// Get material.
							material = chance_of_panel > RandomFloat() ? panel : block;
							color = 0x00'FF'FF'FF & RandomUInt();

							// Build floor.
							for (int h = 0; h < scale; ++h, ++y)
							{
								for (int w = 0; w < scale; ++w)
								{
									for (int l = 0; l < scale; ++l)
									{
										world.set(x_origin + w, y, z_origin + l, material, color);
									}
								}
							}
						}

						// Stop adding floors, move to next cell.
						else
						{
							break;
						}
					}
				}
			}
		}
	}
};