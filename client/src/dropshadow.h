#ifndef MIDLESS_CLIENT_DROPSHADOW_H
#define MIDLESS_CLIENT_DROPSHADOW_H

#include "raylib.h"

/* Draw one soft blob shadow at the given feet position. */
void DropShadow_Draw(Vector3 position, float radius, float heightAboveGround);
/* Draw shadows for the player and every world entity. */
void DropShadow_DrawAll(void);

#endif
