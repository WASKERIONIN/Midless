#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "blackhole.h"

#ifndef RL_TRIANGLES
#define RL_TRIANGLES 0x0004
#endif
#ifndef RL_QUADS
#define RL_QUADS 0x0007
#endif

/* Static black hole in a fixed sky direction. The accretion ring does not spin. */
static Vector3 HoleDirection(void) {
    return Vector3Normalize((Vector3){0.28f, 0.62f, 0.42f});
}

void BlackHole_Init(void) {}
void BlackHole_Shutdown(void) {}

static void DrawAnnulus(Vector3 center, Vector3 right, Vector3 up, float inner, float outer,
                        int segments, Color innerColor, Color outerColor) {
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / (float)segments * 2.0f * PI;
        float a1 = (float)(i + 1) / (float)segments * 2.0f * PI;
        Vector3 r0 = Vector3Add(Vector3Scale(right, cosf(a0)), Vector3Scale(up, sinf(a0)));
        Vector3 r1 = Vector3Add(Vector3Scale(right, cosf(a1)), Vector3Scale(up, sinf(a1)));
        Vector3 i0 = Vector3Add(center, Vector3Scale(r0, inner));
        Vector3 i1 = Vector3Add(center, Vector3Scale(r1, inner));
        Vector3 o0 = Vector3Add(center, Vector3Scale(r0, outer));
        Vector3 o1 = Vector3Add(center, Vector3Scale(r1, outer));

        rlColor4ub(innerColor.r, innerColor.g, innerColor.b, innerColor.a);
        rlVertex3f(i0.x, i0.y, i0.z);
        rlColor4ub(outerColor.r, outerColor.g, outerColor.b, outerColor.a);
        rlVertex3f(o0.x, o0.y, o0.z);
        rlVertex3f(o1.x, o1.y, o1.z);

        rlColor4ub(innerColor.r, innerColor.g, innerColor.b, innerColor.a);
        rlVertex3f(i0.x, i0.y, i0.z);
        rlColor4ub(outerColor.r, outerColor.g, outerColor.b, outerColor.a);
        rlVertex3f(o1.x, o1.y, o1.z);
        rlColor4ub(innerColor.r, innerColor.g, innerColor.b, innerColor.a);
        rlVertex3f(i1.x, i1.y, i1.z);
    }
    rlEnd();
}

void BlackHole_Draw(Vector3 cameraPosition) {
    Vector3 dir = HoleDirection();
    Vector3 center = Vector3Add(cameraPosition, Vector3Scale(dir, 240.0f));

    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(up, dir));
    if (Vector3Length(right) < 0.001f) right = (Vector3){1, 0, 0};
    up = Vector3Normalize(Vector3CrossProduct(dir, right));

    /* Tilt the disk slightly so it reads as a ring, not a circle, and keep it still. */
    Vector3 diskRight = Vector3Normalize(Vector3Add(right, Vector3Scale(dir, 0.15f)));
    Vector3 diskUp = Vector3Normalize(Vector3CrossProduct(dir, diskRight));
    diskRight = Vector3Normalize(Vector3CrossProduct(diskUp, dir));

    rlDrawRenderBatchActive();
    rlSetTexture(0);
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();

    DrawAnnulus(center, diskRight, diskUp, 22.0f, 52.0f, 48,
                (Color){255, 210, 120, 230}, (Color){255, 90, 40, 0});
    DrawAnnulus(center, diskRight, diskUp, 18.5f, 24.0f, 40,
                (Color){255, 240, 200, 255}, (Color){255, 140, 50, 180});

    /* Photon ring — a thin bright halo in the plane of the sky. */
    DrawAnnulus(center, right, up, 16.6f, 19.2f, 36,
                (Color){255, 180, 80, 220}, (Color){255, 80, 40, 40});

    DrawSphere(center, 16.4f, (Color){0, 0, 0, 255});

    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableDepthTest();
}
