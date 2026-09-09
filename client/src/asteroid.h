#ifndef MIDLESS_CLIENT_ASTEROID_H
#define MIDLESS_CLIENT_ASTEROID_H

#include "raylib.h"

void Asteroid_Init(void);
void Asteroid_Shutdown(void);
void Asteroid_Update(float deltaTime);
void Asteroid_Draw(Vector3 cameraPosition, float sunlightStrength);

#endif
