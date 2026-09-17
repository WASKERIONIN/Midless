/*
 * Midless: Cosmic Edition - pocket universe atmosphere (v65.8).
 * v65.28: two pockets - the meadow and the Foundry parkour course.
 * See pocketfx.h for the sync contract with mods/cosmic_islands.lua.
 */
#include "pocketfx.h"
#include <math.h>
#include "raylib.h"

static float pocketFactor;
static float pocketFactor2;

/* Chebyshev distance matches the mod's square pocket_zone boxes, with a
 * soft 16-block fade so flying to the zone edge dissolves the cosmos
 * instead of flipping it; teleport arrivals start deep inside. */
static float Zone_Target(Vector3 pos, float cx, float cz) {
    float dx = fabsf(pos.x - cx);
    float dz = fabsf(pos.z - cz);
    float distance = fmaxf(dx, dz);
    return 1.0f - fminf(fmaxf((distance - (POCKETFX_ZONE_HALF - 16.0f)) / 16.0f, 0.0f), 1.0f);
}

static void Zone_Smooth(float *factor, float target) {
    /* exponential smoothing, frame-rate independent (rate ~6/s) */
    float rate = 1.0f - expf(-6.0f * GetFrameTime());
    if (!isfinite(rate) || rate < 0.0f || rate > 1.0f) rate = 1.0f;  /* v65.9.1 */
    *factor += (target - *factor) * rate;
    if (fabsf(*factor - target) < 0.001f) *factor = target;
    if (!isfinite(*factor)) *factor = target;                        /* v65.9.1 */
}

void PocketFx_Update(Vector3 playerPosition) {
    float previous = pocketFactor;
    float previous2 = pocketFactor2;
    Zone_Smooth(&pocketFactor, Zone_Target(playerPosition, POCKETFX_CX, POCKETFX_CZ));
    Zone_Smooth(&pocketFactor2, Zone_Target(playerPosition, POCKETFX2_CX, POCKETFX2_CZ));
    /* v65.9.1: log the crossing so remote debugging knows where we are */
    if ((previous < 0.5f) != (pocketFactor < 0.5f)) {
        TraceLog(LOG_INFO, "pocketfx: meadow factor crossed %.2f at pos %.1f %.1f %.1f",
                 pocketFactor, playerPosition.x, playerPosition.y, playerPosition.z);
    }
    if ((previous2 < 0.5f) != (pocketFactor2 < 0.5f)) {
        TraceLog(LOG_INFO, "pocketfx: foundry factor crossed %.2f at pos %.1f %.1f %.1f",
                 pocketFactor2, playerPosition.x, playerPosition.y, playerPosition.z);
    }
}

float PocketFx_Factor(void) {
    return pocketFactor;
}

float PocketFx_Factor2(void) {
    return pocketFactor2;
}

float PocketFx_FactorAny(void) {
    return pocketFactor > pocketFactor2 ? pocketFactor : pocketFactor2;
}

int PocketFx_ViewZone(void) {
    if (pocketFactor > 0.5f) return 1;
    if (pocketFactor2 > 0.5f) return 2;
    return 0;
}

bool PocketFx_ChunkVisible(float chunkCenterX, float chunkCenterZ) {
    bool in1 = fabsf(chunkCenterX - POCKETFX_CX) < POCKETFX_ZONE_HALF &&
               fabsf(chunkCenterZ - POCKETFX_CZ) < POCKETFX_ZONE_HALF;
    bool in2 = fabsf(chunkCenterX - POCKETFX2_CX) < POCKETFX_ZONE_HALF &&
               fabsf(chunkCenterZ - POCKETFX2_CZ) < POCKETFX_ZONE_HALF;
    int view = PocketFx_ViewZone();
    if (view == 1) return in1;
    if (view == 2) return in2;
    return !in1 && !in2;
}

Color PocketFx_SkyColor(void) {
    /* cosmos: the familiar near-black indigo (main.c ClearBackground) */
    const float cosmic[3] = { 14.0f, 4.0f, 28.0f };
    /* pocket: a bright, calm day sky - pale azure with a warm lift.
     * Nothing in the void looks like this, which is the point. */
    /* v65.16: back to the calm blue of v65.12 - the dreamcore gradient
     * experiment is over, the peristyle frames the meadow instead */
    const float pocket[3] = { 124.0f, 178.0f, 214.0f };
    /* v65.36: the Foundry floats in the SAME starlit void as the main
     * game - the concrete dusk read as empty and plain over custom maps,
     * so zone 2 keeps the cosmic near-black indigo and gets the stars */
    const float foundry[3] = { 14.0f, 4.0f, 28.0f };
    Color sky;
    for (int i = 0; i < 3; i++) {
        float c = cosmic[i] + (pocket[i] - cosmic[i]) * pocketFactor
                            + (foundry[i] - cosmic[i]) * pocketFactor2;
        if (c < 0.0f) c = 0.0f;
        if (c > 255.0f) c = 255.0f;
        ((unsigned char *)&sky.r)[i] = (unsigned char)c;
    }
    sky.a = 255;
    return sky;
}
