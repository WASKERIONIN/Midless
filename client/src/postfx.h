#ifndef MIDLESS_CLIENT_POSTFX_H
#define MIDLESS_CLIENT_POSTFX_H

#include "raylib.h"
#include "blackhole.h"

void PostFx_Init(void);
void PostFx_Shutdown(void);
void PostFx_BeginScene(void);
void PostFx_EndScene(Camera camera);

#endif
