#ifndef MIDLESS_CLIENT_BIRD_H
#define MIDLESS_CLIENT_BIRD_H

#include "raylib.h"

void Bird_Init(void);
void Bird_Shutdown(void);
void Bird_Clear(void);
void Bird_Update(float dt);
void Bird_Draw(void);
void Bird_GetStats(int *total, int *fly, int *sit, int *peck);

#endif
