#ifndef MIDLESS_CLIENT_STARFIELD_H
#define MIDLESS_CLIENT_STARFIELD_H

#include "raylib.h"

void Starfield_Init(void);
void Starfield_Shutdown(void);
void Starfield_Update(float deltaTime);
void Starfield_Draw(Vector3 cameraPosition, float sunlightStrength);

#endif
