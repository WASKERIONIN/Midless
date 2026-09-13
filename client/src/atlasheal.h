/*
 * Midless: Cosmic Edition - terrain atlas self-heal (v65.11).
 *
 * A terrain.png that predates (or never had) the pocket-universe tiles
 * 78-80 made every pocket block render invisible: the client rejected
 * their definitions and meshed nothing. Instead of leaving the player
 * with ghost cubes, patch placeholder pixels for the missing tiles
 * straight into the GPU atlas at startup. The red banner still asks
 * for a proper zip extract; this keeps the game playable meanwhile.
 */
#ifndef ATLASHEAL_H
#define ATLASHEAL_H

#include "raylib.h"

/* Patches blank pocket tiles (78-80) into the loaded terrain atlas. */
void AtlasHeal_Terrain(Texture2D *terrain);

#endif
