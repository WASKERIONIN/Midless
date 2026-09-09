#ifndef MIDLESS_CLIENT_BLACKHOLE_H
#define MIDLESS_CLIENT_BLACKHOLE_H

#include "raylib.h"

typedef struct BlackHoleScreen {
    Vector2 center;   /* screen UV of the black hole center (0..1) */
    float radius;     /* screen UV radius of the photon ring (in uv units, x-scaled) */
    bool visible;     /* whether it projects in front of the camera */
} BlackHoleScreen;

void BlackHole_Init(void);
void BlackHole_Shutdown(void);
void BlackHole_Update(float deltaTime);
void BlackHole_Draw(Camera camera);
BlackHoleScreen BlackHole_GetScreenState(Camera camera);

#endif
