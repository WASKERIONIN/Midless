#include <math.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "starfield.h"

#ifndef RL_LINES
#define RL_LINES 0x0001
#endif

#define STAR_COUNT 1600
#define STAR_RADIUS 260.0f
#define NEBULA_COUNT 10

static Vector3 starPos[STAR_COUNT];
static Color starCol[STAR_COUNT];
static Vector3 nebulaDir[NEBULA_COUNT];
static float nebulaSize[NEBULA_COUNT];
static Color nebulaCol[NEBULA_COUNT];
static Texture2D nebulaTex;
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
        if (roll < 0.50f) {
            starCol[i] = (Color){(unsigned char)(255 * bright), (unsigned char)(236 * bright),
                                 (unsigned char)(210 * bright), 255};
        } else if (roll < 0.82f) {
            starCol[i] = (Color){(unsigned char)(170 * bright), (unsigned char)(220 * bright),
                                 (unsigned char)(255 * bright), 255};
        } else {
            starCol[i] = (Color){(unsigned char)(255 * bright), (unsigned char)(150 * bright),
                                 (unsigned char)(230 * bright), 255};
        }
    }

    Image img = GenImageGradientRadial(128, 128, 0.15f, (Color){255, 255, 255, 170},
                                       (Color){255, 255, 255, 0});
    nebulaTex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(nebulaTex, TEXTURE_FILTER_BILINEAR);

    for (int i = 0; i < NEBULA_COUNT; i++) {
        float u = Unit(Mix(200u + (uint32_t)i * 5u)) * 2.0f - 1.0f;
        float v = Unit(Mix(201u + (uint32_t)i * 5u)) * 1.4f - 0.2f;
        float w = Unit(Mix(202u + (uint32_t)i * 5u)) * 2.0f - 1.0f;
        float len = sqrtf(u * u + v * v + w * w);
        if (len < 0.0001f) len = 1.0f;
        nebulaDir[i] = (Vector3){u / len, v / len, w / len};
        nebulaSize[i] = 38.0f + Unit(Mix(300u + (uint32_t)i)) * 36.0f;
        float kind = Unit(Mix(400u + (uint32_t)i));
        if (kind < 0.34f) nebulaCol[i] = (Color){210, 40, 230, 55};
        else if (kind < 0.67f) nebulaCol[i] = (Color){40, 210, 230, 50};
        else nebulaCol[i] = (Color){160, 50, 255, 48};
    }
    ready = true;
}

void Starfield_Shutdown(void) {
    if (ready) UnloadTexture(nebulaTex);
    ready = false;
}

void Starfield_Update(float deltaTime) { (void)deltaTime; }

void Starfield_Draw(Camera camera) {
    if (!ready) return;

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlSetBlendMode(BLEND_ADDITIVE);

    for (int i = 0; i < NEBULA_COUNT; i++) {
        Vector3 p = Vector3Add(camera.position, Vector3Scale(nebulaDir[i], 210.0f));
        DrawBillboard(camera, nebulaTex, p, nebulaSize[i], nebulaCol[i]);
    }

    rlDrawRenderBatchActive();
    rlBegin(RL_LINES);
    for (int i = 0; i < STAR_COUNT; i++) {
        Vector3 p = Vector3Add(camera.position, starPos[i]);
        float s = (i % 19 == 0) ? 1.5f : 0.45f;
        rlColor4ub(starCol[i].r, starCol[i].g, starCol[i].b, 255);
        rlVertex3f(p.x - s, p.y, p.z);
        rlVertex3f(p.x + s, p.y, p.z);
        rlVertex3f(p.x, p.y - s, p.z);
        rlVertex3f(p.x, p.y + s, p.z);
    }
    rlEnd();

    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthMask();
    rlEnableDepthTest();
}
