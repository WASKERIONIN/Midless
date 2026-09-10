/*
 * Midless: Cosmic Edition — deep-space backdrop.
 *
 * Layers (all camera-locked, slow celestial rotation applied):
 *   - soft nebula billboards in three palettes (magenta / violet / teal)
 *     arranged along a "galaxy band" plus scattered patches
 *   - dense line-star dust (GL_LINES, one batch)
 *   - bright billboard stars with per-star twinkle
 *   - shooting stars that streak across the sky every few seconds
 */

#include <math.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "starfield.h"

#ifndef RL_LINES
#define RL_LINES 0x0001
#endif

#define STAR_COUNT       1500
#define BRIGHT_STAR_COUNT 210
#define NEBULA_COUNT     22
#define SHOOTING_MAX     3
#define STAR_RADIUS      255.0f
typedef struct BrightStar {
    Vector3 pos;
    float size;
    float twinkleSpeed;
    float phase;
    Color color;
} BrightStar;

typedef struct ShootingStar {
    bool active;
    Vector3 pos;
    Vector3 vel;
    float life;
    float maxLife;
} ShootingStar;

static Vector3 starPos[STAR_COUNT];
static Color starCol[STAR_COUNT];
static BrightStar brightStars[BRIGHT_STAR_COUNT];
static Vector3 nebulaDir[NEBULA_COUNT];
static float nebulaSize[NEBULA_COUNT];
static float nebulaPulse[NEBULA_COUNT];
static ShootingStar shooting[SHOOTING_MAX];
static float nextShootingIn = 4.0f;
static Texture2D starTex;
static float skyTime = 0.0f;
static bool ready;

static uint32_t Mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

static float Unit(uint32_t h) { return (h >> 8) * (1.0f / 16777216.0f); }

static Vector3 OnSphere(uint32_t seed) {
    float u = Unit(Mix(seed)) * 2.0f - 1.0f;
    float v = Unit(Mix(seed + 1u)) * 2.0f - 1.0f;
    float w = Unit(Mix(seed + 2u)) * 2.0f - 1.0f;
    float len = sqrtf(u * u + v * v + w * w);
    if (len < 0.0001f) len = 1.0f;
    return (Vector3){ u / len, v / len, w / len };
}

static Color StarColor(uint32_t seed) {
    float bright = 0.55f + Unit(Mix(seed + 90u)) * 0.45f;
    float roll = Unit(Mix(seed + 40u));
    if (roll < 0.46f)
        return (Color){ (unsigned char)(255 * bright), (unsigned char)(238 * bright),
                        (unsigned char)(214 * bright), 255 };
    if (roll < 0.78f)
        return (Color){ (unsigned char)(168 * bright), (unsigned char)(216 * bright),
                        (unsigned char)(255 * bright), 255 };
    return (Color){ (unsigned char)(236 * bright), (unsigned char)(150 * bright),
                    (unsigned char)(255 * bright), 255 };
}

/* ---- v44: wireframe dream polyhedra -----------------------------------
 * In place of painted nebulae: huge, slow-tumbling black & white wireframe
 * solids hanging in the void - pure Y2K vector geometry. */
typedef struct SkyWire {
    int vertexCount;
    int edgeCount;
    const Vector3 *vertices;
    const unsigned char *edges;
} SkyWire;

static const Vector3 WOCTA_V[6] = {
    { 0,  1,  0}, { 0, -1,  0}, { 1, 0, 0}, {-1, 0, 0}, {0, 0,  1}, {0, 0, -1}
};
static const unsigned char WOCTA_E[12][2] = {
    {0,2},{0,3},{0,4},{0,5},{1,2},{1,3},{1,4},{1,5},{2,4},{4,3},{3,5},{5,2}
};
static const Vector3 WICO_V[12] = {
    {-1,  0.618f, 0}, { 1,  0.618f, 0}, {-1, -0.618f, 0}, { 1, -0.618f, 0},
    { 0, -1,  0.618f}, { 0,  1,  0.618f}, { 0, -1, -0.618f}, { 0,  1, -0.618f},
    { 0.618f, 0, -1}, { 0.618f, 0,  1}, {-0.618f, 0, -1}, {-0.618f, 0,  1}
};
static const unsigned char WICO_E[30][2] = {
    {0,1},{0,5},{0,7},{0,10},{0,11},{1,5},{1,7},{1,8},{1,9},{2,3},
    {2,4},{2,6},{2,10},{2,11},{3,4},{3,6},{3,8},{3,9},{4,5},{4,9},
    {4,11},{5,9},{5,11},{6,7},{6,8},{6,10},{7,10},{8,9},{8,10},{9,11}
};
static const SkyWire kSkyWires[2] = {
    { 6, 12, WOCTA_V, &WOCTA_E[0][0] },
    {12, 30, WICO_V,  &WICO_E[0][0] },
};

static Vector3 SkyWireRotate(Vector3 v, float ax, float ay) {
    float cx = cosf(ax), sx = sinf(ax), cy = cosf(ay), sy = sinf(ay);
    float y = v.y * cx - v.z * sx;
    float z = v.y * sx + v.z * cx;
    float x = v.x * cy + z * sy;
    z = -v.x * sy + z * cy;
    return (Vector3){ x, y, z };
}

void Starfield_Init(void) {
    /* galaxy band plane: tilted disk the eye can read as a galactic arm */
    Vector3 bandNormal = Vector3Normalize((Vector3){ 0.42f, 0.86f, -0.28f });
    Vector3 bandU = Vector3Normalize(Vector3CrossProduct(bandNormal, (Vector3){ 0, 1, 0 }));
    Vector3 bandV = Vector3Normalize(Vector3CrossProduct(bandNormal, bandU));

    for (int i = 0; i < STAR_COUNT; i++) {
        Vector3 dir;
        float bandMix = Unit(Mix((uint32_t)i * 5u + 700u));
        if (bandMix < 0.42f) {
            /* clustered along the band with a gaussian-ish spread */
            float ang = Unit(Mix((uint32_t)i * 3u + 1u)) * 6.2831f;
            float spread = (Unit(Mix((uint32_t)i * 3u + 2u)) + Unit(Mix((uint32_t)i * 3u + 5u)) - 1.0f) * 0.34f;
            dir = Vector3Add(Vector3Scale(bandU, cosf(ang) * cosf(spread)),
                             Vector3Scale(bandV, sinf(ang) * cosf(spread)));
            dir = Vector3Add(dir, Vector3Scale(bandNormal, sinf(spread)));
            dir = Vector3Normalize(dir);
        } else {
            dir = OnSphere((uint32_t)i * 3u + 1u);
        }
        starPos[i] = Vector3Scale(dir, STAR_RADIUS);
        starCol[i] = StarColor((uint32_t)i + 40u);
    }

    for (int i = 0; i < BRIGHT_STAR_COUNT; i++) {
        Vector3 dir = OnSphere((uint32_t)i * 7u + 2001u);
        float bandPull = Unit(Mix((uint32_t)i * 11u + 55u));
        if (bandPull < 0.5f) {
            /* pull bright stars toward the band too */
            float along = dir.x * bandU.x + dir.y * bandU.y + dir.z * bandU.z;
            Vector3 projected = Vector3Subtract(dir, Vector3Scale(bandNormal, along * 0.55f));
            dir = Vector3Normalize(projected);
        }
        brightStars[i].pos = Vector3Scale(dir, STAR_RADIUS * 0.995f);
        brightStars[i].size = 1.1f + Unit(Mix((uint32_t)i * 23u + 9u)) * 2.3f;
        brightStars[i].twinkleSpeed = 0.6f + Unit(Mix((uint32_t)i * 31u + 13u)) * 2.4f;
        brightStars[i].phase = Unit(Mix((uint32_t)i * 37u + 17u)) * 6.2831f;
        brightStars[i].color = StarColor((uint32_t)i * 3u + 500u);
    }

    Image glow = GenImageGradientRadial(64, 64, 0.05f, (Color){ 255, 255, 255, 255 },
                                        (Color){ 255, 255, 255, 0 });
    starTex = LoadTextureFromImage(glow);
    UnloadImage(glow);
    SetTextureFilter(starTex, TEXTURE_FILTER_BILINEAR);



    for (int i = 0; i < NEBULA_COUNT; i++) {
        Vector3 dir;
        if (i < 12) {
            float ang = Unit(Mix(2000u + (uint32_t)i * 5u)) * 6.2831f;
            float spread = (Unit(Mix(2100u + (uint32_t)i * 5u)) - 0.5f) * 0.55f;
            dir = Vector3Add(Vector3Scale(bandU, cosf(ang) * cosf(spread)),
                             Vector3Scale(bandV, sinf(ang) * cosf(spread)));
            dir = Vector3Add(dir, Vector3Scale(bandNormal, sinf(spread)));
            dir = Vector3Normalize(dir);
        } else {
            dir = OnSphere(3000u + (uint32_t)i * 5u);
            dir.y *= 0.6f;
            dir = Vector3Normalize(dir);
        }
        nebulaDir[i] = dir;
        nebulaSize[i] = 55.0f + Unit(Mix(300u + (uint32_t)i)) * 85.0f;
        nebulaPulse[i] = Unit(Mix(400u + (uint32_t)i)) * 6.2831f;
    }

    for (int i = 0; i < SHOOTING_MAX; i++) shooting[i].active = false;
ready = true;
}

void Starfield_Shutdown(void) {
    if (!ready) return;
    UnloadTexture(starTex);
    ready = false;
}

void Starfield_Update(float deltaTime) {
    skyTime += deltaTime;

    nextShootingIn -= deltaTime;
    if (nextShootingIn <= 0.0f) {
        nextShootingIn = 6.0f + (Unit(Mix((uint32_t)(skyTime * 37.0f) + 91u))) * 9.0f;
        for (int i = 0; i < SHOOTING_MAX; i++) {
            ShootingStar *s = &shooting[i];
            if (s->active) continue;
            Vector3 dir = OnSphere((uint32_t)(skyTime * 100.0f) + (uint32_t)i * 61u + 7u);
            if (dir.y < 0.05f) dir.y = 0.05f - dir.y * 0.3f;
            dir = Vector3Normalize(dir);
            Vector3 tangent = Vector3Normalize(Vector3CrossProduct(dir, (Vector3){ 0, 1, 0 }));
            s->pos = Vector3Scale(dir, STAR_RADIUS * 0.97f);
            s->vel = Vector3Scale(Vector3Add(Vector3Scale(tangent, 260.0f),
                                             Vector3Scale(dir, 60.0f)), 1.0f);
            s->maxLife = 0.9f + Unit(Mix((uint32_t)(skyTime * 53.0f))) * 0.6f;
            s->life = s->maxLife;
            s->active = true;
            break;
        }
    }
    for (int i = 0; i < SHOOTING_MAX; i++) {
        ShootingStar *s = &shooting[i];
        if (!s->active) continue;
        s->life -= deltaTime;
        if (s->life <= 0.0f) { s->active = false; continue; }
        s->pos = Vector3Add(s->pos, Vector3Scale(s->vel, deltaTime));
        s->pos = Vector3Scale(Vector3Normalize(Vector3Subtract(s->pos, (Vector3){ 0, 0, 0 })),
                              STAR_RADIUS * 0.97f);
    }
}

/* slow celestial rotation about the sky's Y axis */
static Vector3 SkyPoint(Vector3 p) {
    float c = cosf(skyTime * 0.0045f);
    float s = sinf(skyTime * 0.0045f);
    return (Vector3){ p.x * c - p.z * s, p.y, p.x * s + p.z * c };
}

void Starfield_Draw(Camera camera) {
    if (!ready) return;

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlSetBlendMode(BLEND_ADDITIVE);

    /* nebula clouds */
    rlBegin(RL_LINES);
    for (int i = 0; i < NEBULA_COUNT; i++) {
        Vector3 p = Vector3Add(camera.position, Vector3Scale(SkyPoint(nebulaDir[i]), 218.0f));
        float pulse = 0.82f + 0.18f * sinf(skyTime * 0.23f + nebulaPulse[i]);
        (void)pulse;
        const SkyWire *shape = &kSkyWires[i % 2];
        float s = nebulaSize[i] * 0.16f;
        float ax = skyTime * 0.05f + nebulaPulse[i];
        float ay = skyTime * 0.037f - nebulaPulse[i] * 0.7f;
        Vector3 wp[12];
        for (int v2 = 0; v2 < shape->vertexCount; v2++) {
            Vector3 rv = SkyWireRotate(shape->vertices[v2], ax, ay);
            wp[v2] = Vector3Add(p, Vector3Scale(rv, s));
        }
        unsigned char bright = (unsigned char)(70.0f + 50.0f * pulse);
        rlColor4ub(bright, bright, bright, 255);
        for (int e = 0; e < shape->edgeCount; e++) {
            Vector3 a = wp[shape->edges[e * 2]];
            Vector3 b = wp[shape->edges[e * 2 + 1]];
            rlVertex3f(a.x, a.y, a.z);
            rlVertex3f(b.x, b.y, b.z);
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();

    /* faint star dust */
    rlBegin(RL_LINES);
    for (int i = 0; i < STAR_COUNT; i++) {
        Vector3 p = Vector3Add(camera.position, SkyPoint(starPos[i]));
        float twinkle = 0.72f + 0.28f * sinf(skyTime * (1.1f + (i % 7) * 0.23f) + i);
        rlColor4ub((unsigned char)(starCol[i].r * twinkle), (unsigned char)(starCol[i].g * twinkle),
                   (unsigned char)(starCol[i].b * twinkle), 255);
        float s = (i % 23 == 0) ? 1.1f : 0.38f;
        rlVertex3f(p.x - s, p.y, p.z);
        rlVertex3f(p.x + s, p.y, p.z);
        rlVertex3f(p.x, p.y - s, p.z);
        rlVertex3f(p.x, p.y + s, p.z);
    }
    rlEnd();

    /* bright billboard stars with twinkle + cross flare for the largest */
    for (int i = 0; i < BRIGHT_STAR_COUNT; i++) {
        BrightStar *star = &brightStars[i];
        Vector3 p = Vector3Add(camera.position, SkyPoint(star->pos));
        float twinkle = 0.62f + 0.38f * sinf(skyTime * star->twinkleSpeed + star->phase);
        Color c = star->color;
        c.a = (unsigned char)(235 * twinkle);
        DrawBillboard(camera, starTex, p, star->size * (0.85f + 0.3f * twinkle), c);
    }

    /* shooting stars */
    for (int i = 0; i < SHOOTING_MAX; i++) {
        ShootingStar *s = &shooting[i];
        if (!s->active) continue;
        float fade = sinf((1.0f - s->life / s->maxLife) * PI);
        Vector3 tailDir = Vector3Scale(Vector3Normalize(s->vel), -6.5f);
        Vector3 head = Vector3Add(camera.position, s->pos);
        Vector3 tail = Vector3Add(head, tailDir);
        Color c = { 255, 244, 224, (unsigned char)(210 * fade) };
        rlDrawRenderBatchActive();
        rlBegin(RL_LINES);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlVertex3f(head.x, head.y, head.z);
        rlColor4ub(c.r, c.g, c.b, 0);
        rlVertex3f(tail.x, tail.y, tail.z);
        rlEnd();
        DrawBillboard(camera, starTex, head, 3.2f, (Color){ 255, 250, 235, (unsigned char)(200 * fade) });
    }

    rlDrawRenderBatchActive();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthMask();
    rlEnableDepthTest();
}
