#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "voxdecor.h"
#include "voxparse.h"
#include "world.h"
#include "block.h"
#include "pocketfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* One entry per placed model. x/z are world anchors on the starter
 * island (centre 8,8 - the spawn sanctuary); baseY is snapped to the
 * terrain at draw time. `targetH` is the rendered CONTENT height in
 * blocks - the bake measures the model's real occupied height (SIZE
 * includes empty margin, e.g. characters are only ~15 of their 20
 * voxels) and derives blocks-per-voxel from it, so every figure is
 * scaled to the player (collision box 1.5, eyes at 1.5): characters
 * ~1.7 blocks, the T-Rex 2.4, the dragon 7. Nothing ends up dwarf-
 * sized or colossal. hover > 0 floats the model above the ground. */
typedef struct {
    const char *file;
    float x, z;
    float targetH;
    float yawDeg;
    float hover;
    bool spin, bob;
    /* runtime */
    bool loaded, failed, resolved;
    int snapTries;
    double nextSnap;
    float baseY;
    float scale;      /* derived: targetH / content height */
    Mesh mesh;
} VoxPlacement;

static VoxPlacement places[] = {
    /*  file            x      z    targetH  yaw  hover spin  bob */
    { "chr_knight.vox",  16.0f, 10.0f, 1.70f, 200.0f, 0.0f, false, false },
    { "chr_man.vox",     19.0f, 13.0f, 1.70f, 160.0f, 0.0f, false, false },
    { "chr_gumi.vox",    13.0f, 15.0f, 1.70f, 250.0f, 0.0f, false, false },
    { "chr_sword.vox",   11.0f,  6.0f, 1.70f, 120.0f, 0.0f, false, false },
    { "chr_cat.vox",     18.0f,  6.0f, 1.30f,  90.0f, 0.0f, false, false },
    { "chr_fox.vox",    -18.0f, 20.0f, 1.30f,  60.0f, 0.0f, false, false },
    { "T-Rex.vox",      -14.0f, 14.0f, 2.40f,  30.0f, 0.0f, true,  false },
    { "deer.vox",        -8.0f, 22.0f, 1.50f, 300.0f, 0.0f, false, false },
    { "teapot.vox",      26.0f, 18.0f, 2.00f, 200.0f, 0.0f, false, false },
    { "monu9.vox",        2.0f,-24.0f, 5.00f,  20.0f, 0.0f, false, false },
    { "monu0.vox",      -18.0f,-12.0f, 6.00f, 160.0f, 0.0f, false, false },
    { "dragon.vox",      24.0f, -8.0f, 7.00f,   0.0f, 12.0f, true, true },
};
#define VOXDECOR_COUNT ((int)(sizeof(places) / sizeof(places[0])))

static Material voxMat;
static bool voxMatReady = false;

static bool VoxDecor_Solid(Vector3 cell) {
    const Block *b = Block_GetDefinition(World_GetBlock(cell));
    return b && b->colliderType == BLOCK_COLLIDER_SOLID;
}

/* Bake a face-culled, per-face-shaded mesh. Voxel space is z-up; model
 * space is y-up: mx = x - sx/2, my = z, mz = y - sy/2. Non-indexed
 * (6 verts/face) so big scans stay under no index limit. */
static bool VoxDecor_Bake(VoxPlacement *p, const char *path) {
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
    /* the rendered height is the CONTENT height; the base is normalized
     * so models with empty margin underneath (monu0 starts at z=16)
     * still stand ON the ground instead of hovering */
    int contentH = maxz - minz + 1;
    p->scale = p->targetH / (float)contentH;

    /* pass 1: count exposed faces */
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
        /* model-space cube corners: y is up (voxel z) */
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
            Vector3 q[4];
            Vector3 nrm;
            float shade;
            switch (d) {
            case 0: /* +x */
                q[0] = (Vector3){ x1, y0, z0 }; q[1] = (Vector3){ x1, y0, z1 };
                q[2] = (Vector3){ x1, y1, z1 }; q[3] = (Vector3){ x1, y1, z0 };
                nrm = (Vector3){ 1, 0, 0 }; shade = 0.78f; break;
            case 1: /* -x */
                q[0] = (Vector3){ x0, y0, z1 }; q[1] = (Vector3){ x0, y0, z0 };
                q[2] = (Vector3){ x0, y1, z0 }; q[3] = (Vector3){ x0, y1, z1 };
                nrm = (Vector3){ -1, 0, 0 }; shade = 0.78f; break;
            case 2: /* +y (voxel +y = model +z depth) */
                q[0] = (Vector3){ x0, y0, z1 }; q[1] = (Vector3){ x1, y0, z1 };
                q[2] = (Vector3){ x1, y1, z1 }; q[3] = (Vector3){ x0, y1, z1 };
                nrm = (Vector3){ 0, 0, 1 }; shade = 0.62f; break;
            case 3: /* -y */
                q[0] = (Vector3){ x1, y0, z0 }; q[1] = (Vector3){ x0, y0, z0 };
                q[2] = (Vector3){ x0, y1, z0 }; q[3] = (Vector3){ x1, y1, z0 };
                nrm = (Vector3){ 0, 0, -1 }; shade = 0.62f; break;
            case 4: /* +z voxel = model up */
                q[0] = (Vector3){ x0, y1, z0 }; q[1] = (Vector3){ x1, y1, z0 };
                q[2] = (Vector3){ x1, y1, z1 }; q[3] = (Vector3){ x0, y1, z1 };
                nrm = (Vector3){ 0, 1, 0 }; shade = 1.0f; break;
            default: /* down */
                q[0] = (Vector3){ x0, y0, z1 }; q[1] = (Vector3){ x1, y0, z1 };
                q[2] = (Vector3){ x1, y0, z0 }; q[3] = (Vector3){ x0, y0, z0 };
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

    p->mesh = (Mesh){ 0 };
    p->mesh.vertexCount = fv * 6;
    p->mesh.triangleCount = fv;
    p->mesh.vertices = (float *)verts;
    p->mesh.normals = (float *)norms;
    p->mesh.colors = (unsigned char *)cols;
    UploadMesh(&p->mesh, false);
    /* CPU copies can go - the GPU has everything */
    free(verts); free(norms); free(cols);
    p->mesh.vertices = NULL; p->mesh.normals = NULL; p->mesh.colors = NULL;
    return true;
}

void VoxDecor_Draw(Vector3 camPos) {
    /* pocket purity: the meadow and the Foundry never see these */
    if (PocketFx_FactorAny() >= 0.5f) return;
    if (!voxMatReady) { voxMat = LoadMaterialDefault(); voxMatReady = true; }
    double now = GetTime();
    int bakeBudget = 1;   /* one model per frame - no load-in hitch */

    for (int k = 0; k < VOXDECOR_COUNT; k++) {
        VoxPlacement *p = &places[k];
        if (p->failed) continue;
        float dx = p->x + 0.5f - camPos.x, dz = p->z + 0.5f - camPos.z;
        if (dx * dx + dz * dz > 200.0f * 200.0f) continue;

        if (!p->loaded) {
            if (bakeBudget <= 0) continue;
            char path[700];
            snprintf(path, sizeof(path), "%smodels/vox/%s",
                     GetApplicationDirectory(), p->file);
            if (!VoxDecor_Bake(p, path)) { p->failed = true; continue; }
            p->loaded = true;
            bakeBudget--;
        }

        if (!p->resolved) {
            if (now < p->nextSnap) continue;
            p->nextSnap = now + 0.5;
            p->snapTries++;
            int found = -1;
            for (int y = 130; y >= 30; y--) {
                if (VoxDecor_Solid((Vector3){ p->x, (float)y, p->z })) {
                    found = y + 1;
                    break;
                }
            }
            if (found >= 0) { p->baseY = (float)found; p->resolved = true; }
            else if (p->snapTries > 240) { p->failed = true; }
            if (!p->resolved) continue;
        }

        float y = p->baseY + p->hover;
        float yaw = p->yawDeg;
        if (p->spin) yaw += (float)(now * 9.0);          /* slow turntable */
        if (p->bob) y += 0.45f * sinf((float)now * 0.9f);

        rlPushMatrix();
        rlTranslatef(p->x + 0.5f, y, p->z + 0.5f);
        rlRotatef(yaw, 0.0f, 1.0f, 0.0f);
        rlScalef(p->scale, p->scale, p->scale);
        DrawMesh(p->mesh, voxMat, MatrixIdentity());
        rlPopMatrix();
    }
}

void VoxDecor_Unload(void) {
    for (int k = 0; k < VOXDECOR_COUNT; k++) {
        if (places[k].loaded) { UnloadMesh(places[k].mesh); places[k].loaded = false; }
        places[k].resolved = false;
        places[k].snapTries = 0;
    }
    if (voxMatReady) { UnloadMaterial(voxMat); voxMatReady = false; }
}
