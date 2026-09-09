#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "blackhole.h"

void BlackHole_Init(void) {}
void BlackHole_Shutdown(void) {}

void BlackHole_Draw(Vector3 cameraPosition) {
    Vector3 dir = Vector3Normalize((Vector3){0.35f, 0.55f, 0.40f});
    Vector3 center = Vector3Add(cameraPosition, Vector3Scale(dir, 90.0f));

    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();

    DrawSphere(center, 7.2f, (Color){255, 140, 40, 50});
    DrawCylinder(center, 11.0f, 11.0f, 0.7f, 20, (Color){255, 170, 55, 220});
    DrawSphere(center, 5.2f, (Color){0, 0, 0, 255});

    rlEnableDepthMask();
    rlEnableDepthTest();
}
