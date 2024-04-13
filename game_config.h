#pragma once

// Voxel size.
#define WORLDSIZE 64 // power of 2. Warning: max 512 for a 512x512x512x4 bytes = 512MB world!
#define VOXELSIZE 1.0f / WORLDSIZE

// Stretch screen by the provided multiple. Ignored on 0.
#define STRETCHWINDOW 4

// Screen resolution.
#define SCREENSIZE 0

#if SCREENSIZE == 0			// ILO size
#define SCRWIDTH	256
#define SCRHEIGHT	212
#define TILE_SIZE	4

#elif SCREENSIZE == 1
#define SCRWIDTH	512
#define SCRHEIGHT	320
#define TILE_SIZE	8

#elif SCREENSIZE == 2
#define SCRWIDTH	768
#define SCRHEIGHT	480

#elif SCREENSIZE == 3
#define SCRWIDTH	1024
#define SCRHEIGHT	640
#endif

#ifndef TILE_SIZE
#define TILE_SIZE 2
#endif