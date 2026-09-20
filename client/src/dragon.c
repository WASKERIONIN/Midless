#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "dragon.h"
#include "voxparse.h"
#include "world.h"
#include "block.h"
#include "pocketfx.h"
#include "player.h"
#include "mobs.h"
#include "networkhandler.h"
#include "packet.h"
#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define DRAGON_GOLD_ID     24     /* Gold Plate (Lua-defined, warm gold) */
#define DRAGON_HOARD_LIFE  600    /* seconds: 10 minutes to mine it */
#define DRAGON_FLIGHT_TIME 85.0f  /* seconds circling per appearance */
#define DRAGON_TARGET_H    7.0f   /* rendered height in blocks */
#define DRAGON_HOVER       12.0f  /* blocks above the island surface */
#define DRAGON_CIRCLE_R    10.0f
#define HOARD_MAX          256
#define HOARD_FILE         "dragonhoard.dat"

typedef struct {
    int x, y, z;
    unsigned char orig;
    long long deadline;   /* unix seconds */
} HoardRec;

typedef enum { DR_DORMANT, DR_SEEK, DR_ACTIVE } DragonState;

static DragonState dstate = DR_DORMANT;
static double stateUntil = 25.0;   /* first hunt soon after world entry */
static Vector3 anchor = { 0, 0, 0 };
static float anchorTop = 0.0f;
static int gildedNow = 0;

static HoardRec hoard[HOARD_MAX];
static int hoardCount = 0;
static bool hoardLoaded = false;
static time_t nextHoardTick = 0;

static Mesh mesh;
static bool meshReady = false, meshFailed = false;
static Material mat;
static bool matReady = false;
static float meshScale = 0.08f;

/* ------------------------------------------------------------------ util */

static bool Dragon_Solid(float x, float y, float z) {
    const Block *b = Block_GetDefinition(World_GetBlock((Vector3){ x, y, z }));
    return b && b->colliderType == BLOCK_COLLIDER_SOLID;
}

static bool Dragon_NearPocket(float x, float z) {
    /* pocket 1 meadow (1200,-1200) and pocket 2 Foundry (-1200,1200)
     * keep their purity - the dragon never gilds inside them */
    if (fabsf(x - 1200.0f) < 220.0f && fabsf(z + 1200.0f) < 220.0f) return true;
    if (fabsf(x + 1200.0f) < 220.0f && fabsf(z - 1200.0f) < 220.0f) return true;
    return false;
}

/* natural terrain blocks only - never gild a machine, a plant, a gate
 * or anything the player built out of crafted materials */
static bool Dragon_NaturalId(int id) {
    return (id >= 1 && id <= 18) || id == 78;   /* base set + meadow turf */
}

/* ------------------------------------------------------- mesh (fixed) */

/* Bake dragon.vox into a face-culled mesh. v65.41 fix: the v65.40 baker
 * wound 4 of the 6 face directions CLOCKWISE, so backface culling ate
 * the top/bottom/left/right faces - models looked like they had missing
 * cube sides and came apart into slices. All quads below are CCW seen
 * from OUTSIDE (verified by cross product of the first two edges). */
static bool Dragon_Bake(void) {
    char path[700];
    snprintf(path, sizeof(path), "%smodels/vox/dragon.vox", GetApplicationDirectory());
    VoxData v;
    if (!VoxParseFile(path, &v)) return false;

    size_t total = (size_t)v.sx * (size_t)v.sy * (size_t)v.sz;
    unsigned char *occ = calloc(total, 1);
    if (!occ) { VoxFree(&v); return false; }
    int minx = v.sx, maxx = -1, miny = v.sy, maxy = -1, minz = v.sz, maxz = -1;
    for (int i = 0; i < v.count; i++) {
        int x = v.vox[i][0], y = v.vox[i][1], z = v.vox[i][2];
        if (x < 0 || y < 0 || z < 0 || x >= v.sx || y >= v.sy || z >= v.sz)
            continue;
        occ[((size_t)z * v.sy + y) * v.sx + x] = 1;
        if (x < minx) minx = x; if (x > maxx) maxx = x;
        if (y < miny) miny = y; if (y > maxy) maxy = y;
        if (z < minz) minz = z; if (z > maxz) maxz = z;
    }
    if (maxx < 0) { free(occ); VoxFree(&v); return false; }
    int contentH = maxz - minz + 1;
    meshScale = DRAGON_TARGET_H / (float)contentH;

    int faces = 0;
    for (int i = 0; i < v.count; i++) {
        int x = v.vox[i][0], y = v.vox[i][1], z = v.vox[i][2];
        if (x < 0 || y < 0 || z < 0 || x >= v.sx || y >= v.sy || z >= v.sz)
            continue;
        for (int d = 0; d < 6; d++) {
            int nx = x + (d == 0) - (d == 1);
            int ny = y + (d == 2) - (d == 3);
            int nz = z + (d == 4) - (d == 5);
            if (nx < 0 || ny < 0 || nz < 0 || nx >= v.sx || ny >= v.sy || nz >= v.sz) { faces++; continue; }
            if (!occ[((size_t)nz * v.sy + ny) * v.sx + nx]) faces++;
        }
    }
    if (faces <= 0) { free(occ); VoxFree(&v); return false; }

    Vector3 *verts = malloc((size_t)faces * 6 * sizeof(Vector3));
    Vector3 *norms = malloc((size_t)faces * 6 * sizeof(Vector3));
    Color *cols = malloc((size_t)faces * 6 * sizeof(Color));
    if (!verts || !norms || !cols) {
        free(verts); free(norms); free(cols); free(occ); VoxFree(&v);
        return false;
    }

    float ox = (minx + maxx + 1) * 0.5f, oy = (miny + maxy + 1) * 0.5f;
    int fv = 0;
    for (int i = 0; i < v.count; i++) {
        int x = v.vox[i][0], y = v.vox[i][1], z = v.vox[i][2];
        if (x < 0 || y < 0 || z < 0 || x >= v.sx || y >= v.sy || z >= v.sz)
            continue;
        int pi = v.vox[i][3];
        const uint8_t *rgba = v.palette[pi > 0 ? pi - 1 : 0];
        Color base = { rgba[0], rgba[1], rgba[2], rgba[3] };
        /* model space: y is UP (voxel z), content base normalized to 0 */
        float x0 = x - ox, x1 = x0 + 1.0f;
        float y0 = (float)(z - minz), y1 = y0 + 1.0f;
        float z0 = y - oy, z1 = z0 + 1.0f;
        for (int d = 0; d < 6; d++) {
            int nx = x + (d == 0) - (d == 1);
            int ny = y + (d == 2) - (d == 3);
            int nz = z + (d == 4) - (d == 5);
            bool open;
            if (nx < 0 || ny < 0 || nz < 0 || nx >= v.sx || ny >= v.sy || nz >= v.sz) open = true;
            else open = !occ[((size_t)nz * v.sy + ny) * v.sx + nx];
            if (!open) continue;
            /* CCW seen from outside - cross(edge01, edge12) == outward */
            Vector3 q[4];
            Vector3 nrm;
            float shade;
            switch (d) {
            case 0: /* +x right */
                q[0] = (Vector3){ x1, y0, z0 }; q[1] = (Vector3){ x1, y1, z0 };
                q[2] = (Vector3){ x1, y1, z1 }; q[3] = (Vector3){ x1, y0, z1 };
                nrm = (Vector3){ 1, 0, 0 }; shade = 0.78f; break;
            case 1: /* -x left */
                q[0] = (Vector3){ x0, y0, z1 }; q[1] = (Vector3){ x0, y1, z1 };
                q[2] = (Vector3){ x0, y1, z0 }; q[3] = (Vector3){ x0, y0, z0 };
                nrm = (Vector3){ -1, 0, 0 }; shade = 0.78f; break;
            case 2: /* +z depth */
                q[0] = (Vector3){ x0, y0, z1 }; q[1] = (Vector3){ x1, y0, z1 };
                q[2] = (Vector3){ x1, y1, z1 }; q[3] = (Vector3){ x0, y1, z1 };
                nrm = (Vector3){ 0, 0, 1 }; shade = 0.62f; break;
            case 3: /* -z depth */
                q[0] = (Vector3){ x1, y0, z0 }; q[1] = (Vector3){ x0, y0, z0 };
                q[2] = (Vector3){ x0, y1, z0 }; q[3] = (Vector3){ x1, y1, z0 };
                nrm = (Vector3){ 0, 0, -1 }; shade = 0.62f; break;
            case 4: /* top (voxel +z) */
                q[0] = (Vector3){ x0, y1, z0 }; q[1] = (Vector3){ x0, y1, z1 };
                q[2] = (Vector3){ x1, y1, z1 }; q[3] = (Vector3){ x1, y1, z0 };
                nrm = (Vector3){ 0, 1, 0 }; shade = 1.0f; break;
            default: /* bottom */
                q[0] = (Vector3){ x0, y0, z1 }; q[1] = (Vector3){ x0, y0, z0 };
                q[2] = (Vector3){ x1, y0, z0 }; q[3] = (Vector3){ x1, y0, z1 };
                nrm = (Vector3){ 0, -1, 0 }; shade = 0.5f; break;
            }
            Color c = { (unsigned char)(base.r * shade), (unsigned char)(base.g * shade),
                        (unsigned char)(base.b * shade), base.a };
            int t0 = fv * 6;
            verts[t0] = q[0]; verts[t0 + 1] = q[1]; verts[t0 + 2] = q[2];
            verts[t0 + 3] = q[0]; verts[t0 + 4] = q[2]; verts[t0 + 5] = q[3];
            for (int k = 0; k < 6; k++) { norms[t0 + k] = nrm; cols[t0 + k] = c; }
            fv++;
        }
    }
    free(occ);
    VoxFree(&v);
    if (fv <= 0) { free(verts); free(norms); free(cols); return false; }

    mesh = (Mesh){ 0 };
    mesh.vertexCount = fv * 6;
    mesh.triangleCount = fv;
    mesh.vertices = (float *)verts;
    mesh.normals = (float *)norms;
    mesh.colors = (unsigned char *)cols;
    UploadMesh(&mesh, false);
    free(verts); free(norms); free(cols);
    mesh.vertices = NULL; mesh.normals = NULL; mesh.colors = NULL;
    return true;
}

/* ------------------------------------------------------- hoard records */

static void Hoard_Path(char *out, int len) {
    snprintf(out, len, "%s%s", GetApplicationDirectory(), HOARD_FILE);
}

static void Hoard_Load(void) {
    hoardLoaded = true;
    char path[700];
    Hoard_Path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char magic[4];
    int version = 0, count = 0;
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "DRGH", 4) != 0) { fclose(f); return; }
    if (fread(&version, sizeof(int), 1, f) != 1 || version != 1) { fclose(f); return; }
    if (fread(&count, sizeof(int), 1, f) != 1 || count < 0 || count > HOARD_MAX) { fclose(f); return; }
    for (int i = 0; i < count; i++) {
        HoardRec r;
        long long deadline;
        if (fread(&r.x, sizeof(int), 1, f) != 1) break;
        if (fread(&r.y, sizeof(int), 1, f) != 1) break;
        if (fread(&r.z, sizeof(int), 1, f) != 1) break;
        if (fread(&r.orig, 1, 1, f) != 1) break;
        if (fread(&deadline, sizeof(long long), 1, f) != 1) break;
        r.deadline = deadline;
        hoard[hoardCount++] = r;
    }
    fclose(f);
}

static void Hoard_Save(void) {
    char path[700];
    Hoard_Path(path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int version = 1;
    fwrite("DRGH", 1, 4, f);
    fwrite(&version, sizeof(int), 1, f);
    fwrite(&hoardCount, sizeof(int), 1, f);
    for (int i = 0; i < hoardCount; i++) {
        fwrite(&hoard[i].x, sizeof(int), 1, f);
        fwrite(&hoard[i].y, sizeof(int), 1, f);
        fwrite(&hoard[i].z, sizeof(int), 1, f);
        fwrite(&hoard[i].orig, 1, 1, f);
        long long deadline = hoard[i].deadline;
        fwrite(&deadline, sizeof(long long), 1, f);
    }
    fclose(f);
}

static void Hoard_SetBlock(Vector3 pos, int id) {
    /* same pattern as the player placing a block: predict locally, then
     * tell the server (it applies + broadcasts, so co-op sees it too) */
    World_SetBlock(pos, id, true);
    Network_Send(Packet_CreateSetBlock((unsigned char)id, pos));
}

static void Hoard_Tick(time_t nowR) {
    if (nowR < nextHoardTick) return;
    nextHoardTick = nowR + 2;
    Vector3 pp = player.position;
    bool changed = false;
    for (int i = 0; i < hoardCount; ) {
        HoardRec *r = &hoard[i];
        float dx = r->x + 0.5f - pp.x, dz = r->z + 0.5f - pp.z;
        if (dx * dx + dz * dz > 128.0f * 128.0f) { i++; continue; }  /* chunks unloaded: wait */
        if (nowR >= r->deadline) {
            Vector3 pos = { (float)r->x, (float)r->y, (float)r->z };
            int cur = World_GetBlock(pos);
            if (cur == DRAGON_GOLD_ID)
                Hoard_SetBlock(pos, r->orig);   /* not mined in time: revert */
            /* mined (cur != gold) -> the loot stays with the player */
            hoard[i] = hoard[--hoardCount];
            changed = true;
            continue;
        }
        i++;
    }
    if (changed) Hoard_Save();
}

/* ------------------------------------------------------- state machine */

static bool Dragon_SeekAnchor(void) {
    Vector3 pp = player.position;
    for (int t = 0; t < 48; t++) {
        float ang = (float)(rand() % 6283) / 1000.0f;
        float r = 28.0f + (float)(rand() % 4400) / 100.0f;   /* 28..72 */
        float ax = floorf(pp.x + cosf(ang) * r);
        float az = floorf(pp.z + sinf(ang) * r);
        if (Dragon_NearPocket(ax, az)) continue;
        /* v65.42: never share the sky with the mushroom rain cloud */
        if (Mobs_ShellActive()) {
            Vector3 sc = Mobs_ShellCenter();
            float ex = ax + 0.5f - sc.x, ez = az + 0.5f - sc.z;
            if (ex * ex + ez * ez < 80.0f * 80.0f) continue;
        }
        int top = -1;
        for (int y = 168; y >= 40; y--)
            if (Dragon_Solid(ax, (float)y, az)) { top = y + 1; break; }
        if (top < 0) continue;   /* empty space - not an island */
        anchor = (Vector3){ ax, 0.0f, az };
        anchorTop = (float)top;
        return true;
    }
    return false;
}

static void Dragon_Gild(time_t nowR) {
    int placed = 0;
    for (int t = 0; t < 80 && placed < 14; t++) {
        if (hoardCount >= HOARD_MAX) break;
        float x = anchor.x + (float)(rand() % 13 - 6);
        float z = anchor.z + (float)(rand() % 13 - 6);
        int surf = -1;
        for (int y = (int)anchorTop + 2; y >= (int)anchorTop - 14; y--)
            if (Dragon_Solid(x, (float)y, z)) { surf = y; break; }
        if (surf < 0) continue;
        Vector3 pos = { floorf(x), (float)surf, floorf(z) };
        int id = World_GetBlock(pos);
        if (!Dragon_NaturalId(id)) continue;
        Hoard_SetBlock(pos, DRAGON_GOLD_ID);
        HoardRec r;
        r.x = (int)pos.x; r.y = (int)pos.y; r.z = (int)pos.z;
        r.orig = (unsigned char)id;
        r.deadline = (long long)nowR + DRAGON_HOARD_LIFE;
        hoard[hoardCount++] = r;
        placed++;
    }
    gildedNow = placed;
    if (placed) Hoard_Save();
}

void Dragon_Update(void) {
    if (PocketFx_FactorAny() >= 0.5f) return;   /* pocket purity */
    if (!hoardLoaded) Hoard_Load();
    time_t nowR = time(NULL);
    Hoard_Tick(nowR);

    if (!meshReady && !meshFailed) {
        if (!matReady) { mat = LoadMaterialDefault(); matReady = true; }
        meshReady = Dragon_Bake();
        meshFailed = !meshReady;
    }

    double now = GetTime();
    switch (dstate) {
    case DR_DORMANT:
        if (now >= stateUntil) dstate = DR_SEEK;
        break;
    case DR_SEEK:
        if (meshReady && Dragon_SeekAnchor()) {
            dstate = DR_ACTIVE;
            stateUntil = now + DRAGON_FLIGHT_TIME;
            Dragon_Gild(nowR);
            if (gildedNow > 0)
                Chat_AddOwnedLine("A dragon wheels above a nearby island - its hoard gilds the stone! Mine the gold within 10 minutes to keep it.");
        } else {
            dstate = DR_DORMANT;
            stateUntil = now + 20.0;   /* no island found yet - retry soon */
        }
        break;
    case DR_ACTIVE:
        if (now >= stateUntil) {
            dstate = DR_DORMANT;
            stateUntil = now + 240.0 + (double)(rand() % 240);   /* 4..8 min */
        }
        break;
    }
}

void Dragon_Draw(Vector3 camPos) {
    if (PocketFx_FactorAny() >= 0.5f) return;
    if (dstate != DR_ACTIVE || !meshReady) return;
    double now = GetTime();
    /* v65.42: HOVER like v65.40 did - the v65.41 circle orbit plus the
     * tangential yaw read as "spinning around a strange axis". The dragon
     * parks above its island, bobs gently and turns on the vertical axis */
    float x = anchor.x + 0.5f;
    float z = anchor.z + 0.5f;
    float y = anchorTop + DRAGON_HOVER + 0.5f * sinf((float)now * 0.9f);
    float dx = x - camPos.x, dz = z - camPos.z;
    if (dx * dx + dz * dz > 240.0f * 240.0f) return;
    float yaw = (float)(now * 9.0);   /* slow turntable, degrees */
    rlPushMatrix();
    rlTranslatef(x, y, z);
    rlRotatef(yaw, 0.0f, 1.0f, 0.0f);
    rlScalef(meshScale, meshScale, meshScale);
    DrawMesh(mesh, mat, MatrixIdentity());
    rlPopMatrix();
}

bool Dragon_GetAnchor(Vector3 *out) {
    if (dstate != DR_ACTIVE) return false;
    if (out) *out = (Vector3){ anchor.x + 0.5f, anchorTop, anchor.z + 0.5f };
    return true;
}

void Dragon_Unload(void) {
    if (meshReady) { UnloadMesh(mesh); meshReady = false; }
    if (matReady) { UnloadMaterial(mat); matReady = false; }
}
