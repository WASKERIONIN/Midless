/*
 * Midless: Cosmic Edition - pocket universe atmosphere (v65.8).
 *
 * The pocket universe is a flat meadow the worldgen mod carves far from
 * the cosmic noise (see mods/cosmic_islands.lua, POCKET_CX/POCKET_CZ).
 * While the player is inside that zone the client swaps the whole mood:
 * bright day sky instead of the void, no nebulae, no black hole, no void
 * traffic, warmer light and a soft colour grade.
 *
 * KEEP IN SYNC WITH THE MOD:
 *   POCKETFX_CX / POCKETFX_CZ / POCKETFX_ZONE_HALF  <->  Lua pocket_zone
 */
#ifndef POCKETFX_H
#define POCKETFX_H

#include "raylib.h"

#define POCKETFX_CX 1200.0f
#define POCKETFX_CZ (-1200.0f)
#define POCKETFX_ZONE_HALF 96.0f

/* Call once per frame with the player position (camera is fine). */
void PocketFx_Update(Vector3 playerPosition);

/* 0 = deep in the cosmos, 1 = inside the pocket universe (smoothed). */
float PocketFx_Factor(void);

/* Background colour: cosmic indigo lerped to a bright day sky. */
Color PocketFx_SkyColor(void);

#endif
