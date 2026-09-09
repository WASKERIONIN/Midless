#include <math.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "starfield.h"

#ifndef RL_LINES
#define RL_LINES 0x0001
#endif

#define STAR_COUNT 900
#define STAR_RADIUS 280.0f

static Vector3 starPos[STAR_COUNT];
static Color starCol[STAR_COUNT];
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
        if (roll < 0.55f) {
            starCol[i] = (Color){(unsigned char)(255 * bright), (unsigned char)(236 * bright),
                                 (unsigned char)(210 * bright), 255};
        } else if (roll < 0.85f) {
            starCol[i] = (Color){(unsigned char)(170 * bright), (unsigned char)(220 * bright),
                                 (unsigned char)(255 * bright), 255};
        } else {
            starCol[i] = (Color){(unsigned char)(255 * bright), (unsigned char)(150 * bright),
                                 (unsigned char)(230 * bright), 255};
        }
    }
    ready = true;
}

void Starfield_Shutdown(void) { ready = false; }
void Starfield_Update(float deltaTime) { (void)deltaTime; }

void Starfield_Draw(Vector3 cameraPosition) {
    if (!ready) return;

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();

    /* Tiny crosses — read as stars, not as giant world-space quads. */
    rlBegin(RL_LINES);
    for (int i = 0; i < STAR_COUNT; i++) {
        Vector3 p = Vector3Add(cameraPosition, starPos[i]);
        float s = (i % 17 == 0) ? 1.6f : 0.55f;
        rlColor4ub(starCol[i].r, starCol[i].g, starCol[i].b, 255);
        rlVertex3f(p.x - s, p.y, p.z);
        rlVertex3f(p.x + s, p.y, p.z);
        rlVertex3f(p.x, p.y - s, p.z);
        rlVertex3f(p.x, p.y + s, p.z);
    }
    rlEnd();

    rlEnableDepthMask();
    rlEnableDepthTest();
}
