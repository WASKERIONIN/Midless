/*
 * Midless: Cosmic Edition — the "sun" of the world is a still black hole.
 *
 * Rendering approach (cheap but AAA-styled, one draw batch per element):
 *   1. Outer violet halo billboard (lensed light, squashed).
 *   2. Accretion disk: dozens of additive hot-spot billboards orbiting on a
 *      tilted plane with Keplerian speeds (inner particles race, outer crawl),
 *      tinted white-gold near the horizon out to deep magenta.
 *   3. Photon ring billboard: the sharp gold ring hugging the horizon.
 *   4. Pure-black event horizon sphere drawn last with depth test off, which
 *      cleanly occludes everything drawn behind it inside the ring.
 *   5. Screen-space gravity lensing happens in the post-FX shader using the
 *      projected center/radius exposed by BlackHole_GetScreenState().
 *
 * All textures are generated procedurally at init — no assets required.
 */

#include <math.h>
#include <stdio.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "blackhole.h"

#define BH_DISTANCE     200.0f
#define BH_HORIZON      13.5f   /* world-space radius of the event horizon    */
#define BH_RING_INNER   1.0f    /* photon ring inner radius (x horizon)       */
#define BH_RING_OUTER   1.42f
#define DISK_INNER      1.55f
#define DISK_OUTER      4.6f
#define DISK_PARTICLES  64

typedef struct DiskParticle {
    float orbit;      /* current angle around the disk plane */
    float speed;      /* radians per second (Keplerian)      */
    float radius;     /* orbit radius in horizon units       */
    float size;       /* billboard size in horizon units     */
    float phase;
} DiskParticle;

static Texture2D ringTex;
static Texture2D haloTex;
static Texture2D spotTex;
static DiskParticle particles[DISK_PARTICLES];
static Vector3 diskNormal = { 0.22f, 0.90f, 0.34f };
static Vector3 diskU = { 0 };
static Vector3 diskV = { 0 };
static Vector3 bhDirection = { 0 };
static float diskTime = 0.0f;
static bool ready = false;

/* ------------------------------------------------------------ utilities -- */
static Color RingGradient(float t) {
    /* t: 0 = inner edge of the photon ring, 1 = outer edge. */
    float peak = 0.30f;
    float d = fabsf(t - peak);
    float intensity = expf(-d * d * 26.0f);
    Color inner = (Color){ 255, 236, 190, 255 };
    Color gold = (Color){ 255, 176, 64, 255 };
    Color orange = (Color){ 255, 96, 40, 255 };
    Color violet = (Color){ 190, 70, 255, 255 };
    Color c = gold;
    if (t < peak) c = inner;
    else if (t < 0.55f) c = gold;
    else if (t < 0.78f) c = orange;
    else c = violet;
    unsigned char alpha = (unsigned char)(235.0f * intensity);
    return (Color){ c.r, c.g, c.b, alpha };
}

static Texture2D MakeRingTexture(int size) {
    Image image = GenImageColor(size, size, BLANK);
    Color *pixels = (Color *)image.data;
    float half = size * 0.5f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x - half + 0.5f) / half;
            float dy = (y - half + 0.5f) / half;
            float r = sqrtf(dx * dx + dy * dy);
            Color c = { 0, 0, 0, 0 };
            if (r < 1.0f && r > 0.18f) {
                float t = (r - 0.18f) / 0.82f;
                c = RingGradient(t);
                /* subtle angular shimmer so the ring is not sterile */
                float shimmer = 0.85f + 0.15f * sinf(atan2f(dy, dx) * 9.0f + r * 22.0f);
                c.a = (unsigned char)(c.a * shimmer);
            }
            pixels[y * size + x] = c;
        }
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

static Texture2D MakeHaloTexture(int size) {
    Image image = GenImageColor(size, size, BLANK);
    Color *pixels = (Color *)image.data;
    float half = size * 0.5f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x - half + 0.5f) / half;
            float dy = (y - half + 0.5f) / half * 1.9f; /* squash vertically */
            float r = sqrtf(dx * dx + dy * dy);
            float intensity = expf(-r * r * 4.2f);
            Color c = {
                (unsigned char)(150 + 80 * intensity),
                (unsigned char)(50 + 40 * intensity),
                (unsigned char)(220),
                (unsigned char)(95.0f * intensity),
            };
            pixels[y * size + x] = c;
        }
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

static Texture2D MakeSpotTexture(int size) {
    Image image = GenImageColor(size, size, BLANK);
    Color *pixels = (Color *)image.data;
    float half = size * 0.5f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x - half + 0.5f) / half;
            float dy = (y - half + 0.5f) / half;
            float r = sqrtf(dx * dx + dy * dy);
            float core = expf(-r * r * 7.0f);
            float glow = expf(-r * r * 2.2f);
            Color c = {
                (unsigned char)(255),
                (unsigned char)(200 + 55 * core),
                (unsigned char)(140 + 90 * glow),
                (unsigned char)(200.0f * core + 70.0f * glow * (1.0f - core)),
            };
            pixels[y * size + x] = c;
        }
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

static float ParticleHash(unsigned int seed) {
    seed = (seed * 747796405u) + 2891336453u;
    seed = ((seed >> ((seed >> 28) + 4u)) ^ seed) * 277803737u;
    return (float)((seed >> 22) & 1023) / 1023.0f;
}

/* ------------------------------------------------------------------ api -- */
void BlackHole_Init(void) {
    ringTex = MakeRingTexture(256);
    haloTex = MakeHaloTexture(256);
    spotTex = MakeSpotTexture(64);

    diskNormal = Vector3Normalize(diskNormal);
    Vector3 helper = fabsf(diskNormal.y) < 0.9f ? (Vector3){ 0, 1, 0 } : (Vector3){ 1, 0, 0 };
    diskU = Vector3Normalize(Vector3CrossProduct(diskNormal, helper));
    diskV = Vector3Normalize(Vector3CrossProduct(diskNormal, diskU));

    for (int i = 0; i < DISK_PARTICLES; i++) {
        float t = (float)i / (DISK_PARTICLES - 1);
        float radius = DISK_INNER + (DISK_OUTER - DISK_INNER) * ParticleHash((unsigned int)i * 7u + 3u);
        radius += 0.30f * ParticleHash((unsigned int)i * 13u + 31u);
        particles[i].radius = radius;
        particles[i].orbit = ParticleHash((unsigned int)i * 3u + 1u) * 6.2831f;
        particles[i].speed = 1.9f / powf(radius, 1.5f);
        particles[i].size = (0.85f + 0.9f * ParticleHash((unsigned int)i * 17u + 5u)) *
                            (1.25f - 0.14f * (radius - DISK_INNER));
        particles[i].phase = ParticleHash((unsigned int)i * 29u + 11u) * 6.2831f;
        (void)t;
    }
    ready = true;
}

void BlackHole_Shutdown(void) {
    if (!ready) return;
    UnloadTexture(ringTex);
    UnloadTexture(haloTex);
    UnloadTexture(spotTex);
    ready = false;
}

void BlackHole_Update(float deltaTime) { diskTime += deltaTime; }

static Color DiskTint(float radius, float flicker) {
    /* white-hot at the inner edge, gold mid, magenta at the rim */
    float t = Clamp((radius - DISK_INNER) / (DISK_OUTER - DISK_INNER), 0.0f, 1.0f);
    int r = (int)(255 - 95 * t);
    int g = (int)(235 - 165 * t * t);
    int b = (int)(180 + 70 * t);
    float boost = 0.72f + 0.28f * flicker;
    return (Color){
        (unsigned char)Clamp(r * boost, 0, 255),
        (unsigned char)Clamp(g * boost, 0, 255),
        (unsigned char)Clamp(b * boost, 0, 255),
        (unsigned char)Clamp(150.0f * (1.0f - 0.55f * t) * (0.75f + 0.25f * flicker), 0, 255)
    };
}

void BlackHole_Draw(Camera camera) {
    if (!ready) return;
    Vector3 dir = Vector3Normalize((Vector3){ 0.30f, 0.40f, 0.86f });
    bhDirection = dir;
    Vector3 center = Vector3Add(camera.position, Vector3Scale(dir, BH_DISTANCE));

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlSetBlendMode(BLEND_ADDITIVE);

    /* 1. violet lensed halo, breathing gently */
    float haloScale = BH_HORIZON * 8.2f + sinf(diskTime * 0.31f) * BH_HORIZON * 0.35f;
    DrawBillboard(camera, haloTex, center, haloScale, (Color){ 170, 60, 255, 70 });
    DrawBillboard(camera, haloTex, center, haloScale * 0.55f, (Color){ 235, 90, 255, 60 });

    /* 2. accretion disk particles on the tilted plane */
    Vector3 u = diskU;
    Vector3 v = diskV;
    for (int i = 0; i < DISK_PARTICLES; i++) {
        DiskParticle *p = &particles[i];
        float angle = p->orbit + diskTime * p->speed;
        float flicker = 0.5f + 0.5f * sinf(diskTime * 2.1f + p->phase);
        Vector3 offset = Vector3Add(Vector3Scale(u, cosf(angle) * p->radius * BH_HORIZON),
                                    Vector3Scale(v, sinf(angle) * p->radius * BH_HORIZON));
        /* foreshorten the plane toward the camera a bit for depth (disk tilt) */
        Vector3 pos = Vector3Add(center, offset);
        pos.y += offset.y * -0.28f; /* fake perspective bend of the disk plane */
        DrawBillboard(camera, spotTex, pos, p->size * BH_HORIZON, DiskTint(p->radius, flicker));
        /* the classic "lensed far side" arc above and below the horizon */
        Vector3 mirrorPos = Vector3Add(center, (Vector3){ offset.x, -offset.y * 0.62f, offset.z });
        DrawBillboard(camera, spotTex, mirrorPos, p->size * BH_HORIZON * 0.62f,
                      DiskTint(p->radius, flicker * 0.7f));
    }

    /* 3. photon ring */
    DrawBillboard(camera, ringTex, center, BH_HORIZON * BH_RING_OUTER * 2.55f, WHITE);

    rlDrawRenderBatchActive();
    rlSetBlendMode(BLEND_ALPHA);

    /* 4. event horizon — pure black, swallows the inner disk edges */
    DrawSphere(center, BH_HORIZON * 0.94f, BLACK);
    DrawSphereWires(center, BH_HORIZON * 0.985f, 8, 6, (Color){ 0, 0, 0, 0 });

    rlEnableDepthMask();
    rlEnableDepthTest();
}

BlackHoleScreen BlackHole_GetScreenState(Camera camera) {
    BlackHoleScreen state = { 0 };
    Vector3 dir = Vector3Normalize((Vector3){ 0.30f, 0.40f, 0.86f });
    Vector3 center = Vector3Add(camera.position, Vector3Scale(dir, BH_DISTANCE));

    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    float f = 1.0f / tanf(camera.fovy * DEG2RAD * 0.5f);
    float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();
    Matrix projection = MatrixPerspective(camera.fovy * DEG2RAD, aspect, 0.01f, 2000.0f);

    /* toward-camera vector so we can reject points behind the viewer */
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));

    Matrix viewProj = MatrixMultiply(view, projection);
    Vector4 clip = {
        center.x * viewProj.m0 + center.y * viewProj.m4 + center.z * viewProj.m8 + viewProj.m12,
        center.x * viewProj.m1 + center.y * viewProj.m5 + center.z * viewProj.m9 + viewProj.m13,
        center.x * viewProj.m2 + center.y * viewProj.m6 + center.z * viewProj.m10 + viewProj.m14,
        center.x * viewProj.m3 + center.y * viewProj.m7 + center.z * viewProj.m11 + viewProj.m15
    };
    if (clip.w <= 0.0f) {
        state.visible = false;
        return state;
    }
    Vector2 uv = { clip.x / clip.w * 0.5f + 0.5f, 1.0f - (clip.y / clip.w * 0.5f + 0.5f) };

    Vector3 edgeWorld = Vector3Add(center, Vector3Scale(camForward, -BH_HORIZON)); /* not exact */
    (void)edgeWorld;
    /* angular radius -> uv radius (small-angle on a perspective camera) */
    float angular = BH_HORIZON * BH_RING_OUTER / BH_DISTANCE; /* radians, approx */
    float uvPerRad = 0.5f * f;                                 /* uv units per radian, y axis */
    state.center = uv;
    state.radius = angular * uvPerRad;
    state.visible = uv.x > -0.6f && uv.x < 1.6f && uv.y > -0.6f && uv.y < 1.6f;
    return state;
}
