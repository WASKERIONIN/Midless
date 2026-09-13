/*
 * Midless: Cosmic Edition - pocket universe atmosphere (v65.8).
 * See pocketfx.h for the sync contract with mods/cosmic_islands.lua.
 */
#include "pocketfx.h"
#include <math.h>
#include "raylib.h"

static float pocketFactor;

void PocketFx_Update(Vector3 playerPosition) {
    float dx = fabsf(playerPosition.x - POCKETFX_CX);
    float dz = fabsf(playerPosition.z - POCKETFX_CZ);
    /* Chebyshev distance matches the mod's square pocket_zone box */
    float distance = fmaxf(dx, dz);
    /* soft 16-block fade so flying to the zone edge dissolves the cosmos
     * instead of flipping it; teleport arrivals start deep inside */
    float target = 1.0f - fminf(fmaxf((distance - (POCKETFX_ZONE_HALF - 16.0f)) / 16.0f, 0.0f), 1.0f);
    /* exponential smoothing, frame-rate independent (rate ~6/s) */
    float rate = 1.0f - expf(-6.0f * GetFrameTime());
    if (!isfinite(rate) || rate < 0.0f || rate > 1.0f) rate = 1.0f;  /* v65.9.1 */
    float previous = pocketFactor;
    pocketFactor += (target - pocketFactor) * rate;
    if (fabsf(pocketFactor - target) < 0.001f) pocketFactor = target;
    if (!isfinite(pocketFactor)) pocketFactor = target;             /* v65.9.1 */
    /* v65.9.1: log the crossing so remote debugging knows where we are */
    if ((previous < 0.5f) != (pocketFactor < 0.5f)) {
        TraceLog(LOG_INFO, "pocketfx: factor crossed %.2f at pos %.1f %.1f %.1f",
                 pocketFactor, playerPosition.x, playerPosition.y, playerPosition.z);
    }
}

float PocketFx_Factor(void) {
    return pocketFactor;
}

Color PocketFx_SkyColor(void) {
    /* cosmos: the familiar near-black indigo (main.c ClearBackground) */
    const float cosmic[3] = { 14.0f, 4.0f, 28.0f };
    /* pocket: a bright, calm day sky - pale azure with a warm lift.
     * Nothing in the void looks like this, which is the point. */
    /* v65.13: dreamcore - dusty pink-lavender over the cloud sea
     * (palette sampled from liminal-space references) */
    const float pocket[3] = { 216.0f, 186.0f, 206.0f };
    Color sky;
    sky.r = (unsigned char)(cosmic[0] + (pocket[0] - cosmic[0]) * pocketFactor);
    sky.g = (unsigned char)(cosmic[1] + (pocket[1] - cosmic[1]) * pocketFactor);
    sky.b = (unsigned char)(cosmic[2] + (pocket[2] - cosmic[2]) * pocketFactor);
    sky.a = 255;
    return sky;
}
