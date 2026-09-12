/* test stub - minimal world surface used by client/src/mobs.c */
#ifndef WORLD_H_STUB
#define WORLD_H_STUB
#include "raylib.h"

int World_GetBlock(Vector3 blockPos);
void World_SetBlock(Vector3 blockPos, int blockId, bool send);
void World_ExplodeAt(Vector3 cell);
Texture2D World_GetTerrainTexture(void);

#endif
