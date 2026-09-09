#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "blackhole.h"

void BlackHole_Init(void) {}
void BlackHole_Shutdown(void) {}

void BlackHole_Draw(Camera camera) {
    Vector3 dir = Vector3Normalize((Vector3){0.18f, 0.42f, 0.88f});
    Vector3 center = Vector3Add(camera.position, Vector3Scale(dir, 210.0f));

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();

    rlSetBlendMode(BLEND_ADDITIVE);
    DrawSphere(center, 28.0f, (Color){255, 90, 20, 28});
    DrawSphere(center, 18.0f, (Color){255, 150, 40, 50});
    DrawCircle3D(center, 22.0f, (Vector3){1, 0, 0}, 90.0f, (Color){255, 170, 40, 220});
    DrawCircle3D(center, 18.0f, (Vector3){1, 0, 0}, 90.0f, (Color){255, 210, 80, 180});
    DrawCircle3D(center, 14.0f, (Vector3){1, 0, 0}, 90.0f, (Color){255, 120, 30, 160});
    rlSetBlendMode(BLEND_ALPHA);

    DrawSphere(center, 11.5f, (Color){0, 0, 0, 255});

    rlEnableDepthMask();
    rlEnableDepthTest();
}
