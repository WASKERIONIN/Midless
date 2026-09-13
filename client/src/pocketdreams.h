/*
 * Midless: Cosmic Edition - pocket universe dreamcore ambience (v65.13).
 *
 * The pocket sky carries a slow sea of pastel clouds ringing the meadow
 * (they also veil its edge - the platform dissolves into cloud instead
 * of a hard rim) plus a few high drifters. Pure client-side billboards,
 * faded by the pocket factor.
 */
#ifndef POCKETDREAMS_H
#define POCKETDREAMS_H

#include "raylib.h"

void PocketDreams_Init(void);
void PocketDreams_DrawSky(float pocketFactor);   /* v65.15 gradient */
void PocketDreams_Draw(Camera camera, float pocketFactor);

#endif
