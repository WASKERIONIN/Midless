/*
 * KEEP IN SYNC WITH THE MOD:
 *   POCKETFX_CX / POCKETFX_CZ / POCKETFX_ZONE_HALF  <->  Lua pocket_zone
 *   POCKETFX2_CX / POCKETFX2_CZ (v65.28)            <->  Lua pocket_zone2
 */
#ifndef POCKETFX_H
#define POCKETFX_H

#include "raylib.h"
#include <stdbool.h>

#define POCKETFX_CX 1200.0f
#define POCKETFX_CZ (-1200.0f)
#define POCKETFX_ZONE_HALF 96.0f
/* v65.24: lawn surface cell (mirrors POCKET_TOP in cosmic_islands.lua) */
#define POCKETFX_TOP 154.0f

/* v65.28: the second pocket - the Foundry parkour course */
#define POCKETFX2_CX (-1200.0f)
#define POCKETFX2_CZ 1200.0f
#define POCKETFX2_TOP 118.0f   /* v65.29: the Foundry sits LOW - worldgen max_y is 160 */

/* Call once per frame with the player position (camera is fine). */
void PocketFx_Update(Vector3 playerPosition);

/* 0 = deep in the cosmos, 1 = inside the pocket universe (smoothed). */
float PocketFx_Factor(void);

/* v65.28: same, for the second pocket (the Foundry). */
float PocketFx_Factor2(void);

/* v65.28: whichever pocket holds the player (max of both factors).
 * Generic "not in the cosmos" gates (asteroids, starfield, mobs, the
 * postfx veil) use this so BOTH pockets stay pure. */
float PocketFx_FactorAny(void);

/* v65.28: 0 = overworld, 1 = meadow pocket, 2 = the Foundry. */
int PocketFx_ViewZone(void);

/* v65.28: chunk-veil rule for a chunk centre - only the zone the player
 * is inside renders; from outside, neither pocket zone renders. */
bool PocketFx_ChunkVisible(float chunkCenterX, float chunkCenterZ);

/* Background colour: cosmic indigo lerped to a bright day sky. */
Color PocketFx_SkyColor(void);

#endif
