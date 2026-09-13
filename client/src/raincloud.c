/* v63.7: rain clouds that water the world and grow mushrooms.
 * A small puffy slate cloud drifts near the player, rains for ~45 s,
 * and every drop that hits a biome lawn may sprout that biome's own
 * mushroom. All quads are camera-facing rlgl primitives. */
#include <math.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "raincloud.h"
#include "world.h"
#include "player.h"
#include "textures.h"
#include "packet.h"
#include "networkhandler.h"

#define RC_MAX_CLOUDS 3
#define RC_MAX_DROPS 160
#define RC_GROW_CHANCE 0.09f

typedef struct RainDrop {
    bool active;
    Vector3 pos;
    float dieY;
    float life;
} RainDrop;

typedef struct RainCloud {
    bool active;
    Vector3 pos;
    float age;
    float life;
    float dripT;
    int growBudget;
    Vector3 lastGrow;
} RainCloud;

static RainCloud clouds[RC_MAX_CLOUDS];
static RainDrop drops[RC_MAX_DROPS];
static float spawnTimer = 14.0f;   /* first cloud arrives quickly */

void RainCloud_Init(void) {
    memset(clouds, 0, sizeof(clouds));
    memset(drops, 0, sizeof(drops));
    spawnTimer = 14.0f;
}

void RainCloud_Shutdown(void) {
    memset(clouds, 0, sizeof(clouds));
    memset(drops, 0, sizeof(drops));
}

/* highest loaded non-air block at x,z at or below startY */
static float RainCloud_SurfaceY(float x, float z, float startY) {
    int y0 = (int)floorf(startY);
    for (int y = y0; y >= y0 - 70 && y >= 1; y--) {
        Vector3 p = { x, (float)y + 0.5f, z };
        if (World_GetBlock(p) != 0) return (float)y;
    }
    return -1000.0f;
}

/* the mushroom species native to a ground block (0 = none) */
static int RainCloud_SpeciesFor(int groundId) {
    if (groundId == 57) return 74;   /* cinder trumpet on ember turf */
    if (groundId == 58) return 75;   /* frost puffball on frost turf */
    if (groundId == 3) return 73;    /* glowcap cluster on classic turf */
    return 0;
}

static void RainCloud_Spawn(Vector3 around) {
    for (int i = 0; i < RC_MAX_CLOUDS; i++) {
        RainCloud *c = &clouds[i];
        if (c->active) continue;
        float ang = (float)GetRandomValue(0, 3599) * 0.001745f;
        float r = 13.0f + (float)GetRandomValue(0, 110) / 10.0f;
        c->pos.x = around.x + cosf(ang) * r;
        c->pos.z = around.z + sinf(ang) * r;
        c->pos.y = around.y + 14.0f + (float)GetRandomValue(0, 60) / 10.0f;
        c->age = 0.0f;
        c->life = 38.0f + (float)GetRandomValue(0, 170) / 10.0f;
        c->dripT = 0.35f;
        c->growBudget = 5;
        c->lastGrow = (Vector3){ 0, -1000, 0 };
        c->active = true;
        return;
    }
}

static void RainCloud_Grow(RainCloud *c, float x, float surfY, float z) {
    if (c->growBudget <= 0) return;
    Vector3 cell = { floorf(x) + 0.5f, surfY + 1.5f, floorf(z) + 0.5f };
    if (Vector3Equals(cell, c->lastGrow)) return;
    if (World_GetBlock(cell) != 0) return;
    Vector3 groundP = { cell.x, surfY + 0.5f, cell.z };
    int species = RainCloud_SpeciesFor(World_GetBlock(groundP));
    if (species == 0) return;
    World_SetBlock(cell, species, true);
    Network_Send(Packet_CreateSetBlock(species, cell));
    c->lastGrow = cell;
    c->growBudget--;
}

void RainCloud_Update(float dt) {
    Vector3 pc = player.camera.position;

    spawnTimer -= dt;
    if (spawnTimer <= 0.0f) {
        int n = 0;
        for (int i = 0; i < RC_MAX_CLOUDS; i++) if (clouds[i].active) n++;
        if (n < RC_MAX_CLOUDS) {
            RainCloud_Spawn(pc);
            spawnTimer = 22.0f + (float)GetRandomValue(0, 180) / 10.0f;
        } else {
            spawnTimer = 6.0f;
        }
    }

    for (int i = 0; i < RC_MAX_CLOUDS; i++) {
        RainCloud *c = &clouds[i];
        if (!c->active) continue;
        c->age += dt;
        c->life -= dt;
        c->pos.x += 1.1f * dt;                  /* wind drift */
        if (c->life <= 0.0f) { c->active = false; continue; }

        c->dripT -= dt;
        if (c->dripT <= 0.0f) {
            c->dripT = 0.16f + (float)GetRandomValue(0, 120) / 1000.0f;
            float dx = (float)GetRandomValue(-35, 35) / 10.0f;
            float dz = (float)GetRandomValue(-35, 35) / 10.0f;
            float x = c->pos.x + dx, z = c->pos.z + dz;
            float surfY = RainCloud_SurfaceY(x, z, c->pos.y);
            if (surfY > -500.0f) {
                for (int d = 0; d < RC_MAX_DROPS; d++) {
                    RainDrop *drop = &drops[d];
                    if (drop->active) continue;
                    drop->active = true;
                    drop->pos = (Vector3){ x, surfY + 1.15f, z };
                    drop->dieY = surfY + 0.70f;
                    drop->life = 0.0f;
                    break;
                }
                if (GetRandomValue(0, 99) < (int)(RC_GROW_CHANCE * 100.0f))
                    RainCloud_Grow(c, x, surfY, z);
            }
        }
    }

    for (int d = 0; d < RC_MAX_DROPS; d++) {
        RainDrop *drop = &drops[d];
        if (!drop->active) continue;
        drop->life += dt;
        drop->pos.y -= 15.0f * dt;
        if (drop->pos.y <= drop->dieY || drop->life > 1.0f)
            drop->active = false;
    }
}

static void RainCloud_Quad(Vector3 center, float dx, float dy,
                           float halfW, float h, Color col) {
    Matrix view = rlGetMatrixModelview();
    Vector3 right = Vector3Normalize((Vector3){ view.m0, view.m4, view.m8 });
    Vector3 base = { center.x + dx, center.y + dy, center.z };
    Vector3 bl = Vector3Subtract(base, Vector3Scale(right, halfW));
    Vector3 br = Vector3Add(base, Vector3Scale(right, halfW));
    Vector3 tl = Vector3Add(bl, (Vector3){ 0, h, 0 });
    Vector3 tr = Vector3Add(br, (Vector3){ 0, h, 0 });
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlVertex3f(bl.x, bl.y, bl.z); rlVertex3f(tl.x, tl.y, tl.z);
    rlVertex3f(tr.x, tr.y, tr.z); rlVertex3f(br.x, br.y, br.z);
    rlVertex3f(br.x, br.y, br.z); rlVertex3f(tr.x, tr.y, tr.z);
    rlVertex3f(tl.x, tl.y, tl.z); rlVertex3f(bl.x, bl.y, bl.z);
}

void RainCloud_Draw(void) {
    rlDrawRenderBatchActive();
    rlSetTexture(0);   /* untextured weather primitives */

    rlBegin(RL_QUADS);
    for (int i = 0; i < RC_MAX_CLOUDS; i++) {
        RainCloud *c = &clouds[i];
        if (!c->active) continue;
        float fade = c->age < 2.0f ? c->age / 2.0f
                   : (c->life < 2.0f ? c->life / 2.0f : 1.0f);
        if (fade > 1.0f) fade = 1.0f;
        Vector3 p = c->pos;
        p.y += sinf(c->age * 0.6f) * 0.4f;
        Color top = { (unsigned char)(104 * 1), (unsigned char)(114 * 1),
                      (unsigned char)(148 * 1), (unsigned char)(232 * fade) };
        Color bot = { 82, 90, 120, (unsigned char)(228 * fade) };
        RainCloud_Quad(p, 0.0f, -0.2f, 2.7f, 1.3f, bot);   /* base slab */
        RainCloud_Quad(p, 0.0f, 0.1f, 2.1f, 2.2f, top);    /* main dome */
        RainCloud_Quad(p, -1.6f, 0.1f, 1.4f, 1.9f, top);
        RainCloud_Quad(p, 1.6f, 0.2f, 1.4f, 1.8f, top);
        RainCloud_Quad(p, -0.6f, 1.0f, 1.7f, 1.6f, top);
        RainCloud_Quad(p, 0.8f, 0.9f, 1.6f, 1.5f, top);
        /* faint rain veil under the cloud */
        Color veil = { 150, 190, 235, (unsigned char)(38 * fade) };
        RainCloud_Quad(p, 0.0f, -0.4f, 2.6f, -7.0f, veil);
    }

    Color dropCol = { 165, 212, 255, 235 };
    for (int d = 0; d < RC_MAX_DROPS; d++) {
        RainDrop *drop = &drops[d];
        if (!drop->active) continue;
        RainCloud_Quad(drop->pos, 0.0f, 0.0f, 0.045f, 0.20f, dropCol);
    }
    rlEnd();

    rlSetTexture(ClientTextures_Get(1).id);   /* restore the atlas batch */
}
