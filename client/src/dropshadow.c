/*
 * Midless: Cosmic Edition v43 — soft blob shadows.
 *
 * Real shadow mapping is out of budget for this engine, but grounding the
 * characters matters: for each entity (and the player in third person) we
 * raycast straight down to the first solid surface and draw a translucent
 * dark disc that fades with height and scene brightness.
 */

#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "world.h"
#include "block.h"
#include "player.h"
#include "dropshadow.h"

#define SHADOW_MAX_DROP 24.0f

static bool FindGround(Vector3 from, Vector3 *outGround) {
    int startX = (int)floorf(from.x);
    int startZ = (int)floorf(from.z);
    int startY = (int)floorf(from.y);
    int endY = startY - (int)SHADOW_MAX_DROP;
    if (endY < -8) endY = -8;

    for (int y = startY; y >= endY; y--) {
        Vector3 pos = { (float)startX, (float)y, (float)startZ };
        const Block *block = Block_GetDefinition(World_GetBlock(pos));
        if (block->colliderType != BLOCK_COLLIDER_SOLID) continue;
        /* height of the top surface (slabs etc.) */
        float top = (float)y + block->maxBB.y / 16.0f;
        outGround->x = from.x;
        outGround->y = top;
        outGround->z = from.z;
        return true;
    }
    return false;
}

void DropShadow_Draw(Vector3 position, float radius, float heightAboveGround) {
    Vector3 ground;
    Vector3 probe = position;
    if (!FindGround(probe, &ground)) return;

    float drop = position.y - ground.y;
    if (drop < 0.0f) drop = 0.0f;
    if (drop > SHADOW_MAX_DROP) return;

    /* fade and shrink as the character floats away from the surface */
    float fade = 1.0f - drop / SHADOW_MAX_DROP;
    fade *= fade;
    float bright = World_GetBrightness((Vector3){ ground.x, ground.y + 0.5f, ground.z });
    unsigned char alpha = (unsigned char)(110.0f * fade * Clamp(bright, 0.15f, 1.0f));
    if (alpha < 3) return;
    float size = radius * (0.55f + 0.45f * fade);

    Color core = { 4, 2, 12, alpha };
    Color rim = { 4, 2, 12, 0 };
    DrawCylinder((Vector3){ ground.x, ground.y + 0.02f, ground.z }, size * 0.35f, size,
                 0.02f, 12, core);
    (void)rim;
    (void)heightAboveGround;
}

void DropShadow_DrawAll(void) {
    /* player */
    Vector3 playerFeet = { player.position.x + 0.5f, player.position.y, player.position.z + 0.5f };
    DropShadow_Draw(playerFeet, 0.55f, 0.0f);

    /* world entities */
    for (int i = 0; i < WORLD_MAX_ENTITIES; i++) {
        Entity *entity = &world.entities[i];
        if (entity->type == 0) continue;
        DropShadow_Draw(entity->position, 0.5f, 0.0f);
    }
}
