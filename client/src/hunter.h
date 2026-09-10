#ifndef MIDLESS_CLIENT_HUNTER_H
#define MIDLESS_CLIENT_HUNTER_H

#include "raylib.h"
#include <stdbool.h>

/* v45: void hunters - wireframe predators drifting in the open void
 * between islands. They chase, they sting, they burst into line fragments. */

void Hunter_Init(void);
void Hunter_Shutdown(void);
void Hunter_Update(float deltaTime);   /* call once per frame, in world */
void Hunter_Draw(void);                /* inside BeginMode3D */

/* player swing at a hunter: returns true if the aim ray hit one in range */
bool Hunter_TryHit(Vector3 origin, Vector3 dir, float maxDist);

/* player touch damage query: true if any hunter is within sting range */
bool Hunter_TouchingPlayer(Vector3 playerPos, Vector3 *pushDir);

int Hunter_AliveCount(void);
int Hunter_GetBounty(void);            /* kills this session */
float Hunter_GetSurgeLevel(void);      /* 0..1 void-tide intensity */
float Hunter_GetSurgeTimeLeft(void);   /* seconds left of the current tide */
float Hunter_GetCalmTimeLeft(void);    /* countdown while the tide warning is up */

#endif
