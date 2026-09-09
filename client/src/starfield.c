#include <math.h>
#include <stdint.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "starfield.h"

#ifndef RL_QUADS
#define RL_QUADS 0x0007
#endif
#ifndef RL_TRIANGLES
#define RL_TRIANGLES 0x0004
#endif

#define STAR_COUNT 1600
#define STAR_RADIUS 420.0f
#define NEBULA_COUNT 8

static Vector3 starPos[STAR_COUNT];
static Color starCol[STAR_COUNT];
static float starSize[STAR_COUNT];
static bool ready;

static uint32_t Mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

static float Unit(uint32_t h) { return (h >> 8) * (1.0f / 16777216.0f); }

void Starfield_Init(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        float u = Unit(Mix((uint32_t)i * 3u + 1u)) * 2.0f - 1.0f;
        float v = Unit(Mix((uint32_t)i * 3u + 2u)) * 2.0f - 1.0f;
        float w = Unit(Mix((uint32_t)i * 3u + 3u)) * 2.0f - 1.0f;
        float len = sqrtf(u * u + v * v + w * w);
        if (len < 0.0001f) len = 1.0f;
        starPos[i] = (Vector3){u / len * STAR_RADIUS, v / len * STAR_RADIUS, w / len * STAR_RADIUS};

        float roll = Unit(Mix((uint32_t)i + 40u));
        float bright = 0.55f + Unit(Mix((uint32_t)i + 90u)) * 0.45f;
        if (roll < 0.5f) {
            starCol[i] = (Color){(unsigned char)(255 * bright), (unsigned char)(236 * bright),
                                 (unsigned char)(210 * bright), 255};
        } else if (roll < 0.82f) {
            starCol[i] = (Color){(unsigned char)(170 * bright), (unsigned char)(220 * bright),
                                 (unsigned char)(255 * bright), 255};
        } else {
            starCol[i] = (Color){(unsigned char)(255 * bright), (unsigned char)(140 * bright),
                                 (unsigned char)(230 * bright), 255};
        }
        starSize[i] = 0.7f + Unit(Mix((uint32_t)i + 140u)) * 2.4f;
    }
    ready = true;
}

void Starfield_Shutdown(void) { ready = false; }
void Starfield_Update(float deltaTime) { (void)deltaTime; }

static void DrawBillboardQuad(Vector3 p, float s, Color c) {
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlVertex3f(p.x - s, p.y - s, p.z);
    rlVertex3f(p.x + s, p.y - s, p.z);
    rlVertex3f(p.x + s, p.y + s, p.z);
    rlVertex3f(p.x - s, p.y + s, p.z);

    rlVertex3f(p.x, p.y - s, p.z - s);
    rlVertex3f(p.x, p.y - s, p.z + s);
    rlVertex3f(p.x, p.y + s, p.z + s);
    rlVertex3f(p.x, p.y + s, p.z - s);
}

void Starfield_Draw(Vector3 cameraPosition) {
    if (!ready) return;

    rlDrawRenderBatchActive();
    rlSetTexture(0);
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlPushMatrix();
    rlTranslatef(cameraPosition.x, cameraPosition.y, cameraPosition.z);

    static const Color nebula[NEBULA_COUNT] = {
        {75, 20, 140, 28},  {30, 90, 150, 24},  {180, 40, 130, 26}, {20, 160, 150, 18},
        {120, 30, 180, 22}, {40, 50, 160, 20}, {220, 80, 160, 16}, {10, 20, 60, 30},
    };
    static const Vector3 nebulaPos[NEBULA_COUNT] = {
        { 180,  40, -80}, {-160, -20, 120}, { 40, 130, 160}, {-90,  70, -170},
        {  20, -110, 40}, { 150,  90,  90}, {-40,  20, 200}, {  0, -40, -210},
    };

    rlBegin(RL_QUADS);
    for (int i = 0; i < NEBULA_COUNT; i++) {
        DrawBillboardQuad(nebulaPos[i], 140.0f + (float)(i % 3) * 30.0f, nebula[i]);
    }
    for (int i = 0; i < STAR_COUNT; i++) {
        DrawBillboardQuad(starPos[i], starSize[i], starCol[i]);
    }
    rlEnd();

    rlPopMatrix();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableDepthTest();
}
