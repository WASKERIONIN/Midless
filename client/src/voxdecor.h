#ifndef MIDLESS_VOXDECOR_H
#define MIDLESS_VOXDECOR_H
/* v65.40: sub-block .vox decorations in the MAIN world (starter island).
 * The world grid stays 1 block for collision/worldgen, but these models
 * render at 1/12-block voxel resolution - scaled so a character model
 * stands ~1.7 blocks tall, matching the player. Models by ephtracy
 * (MagicaVoxel samples), see models/CREDITS.txt. */
#include "raylib.h"

void VoxDecor_Draw(Vector3 camPos);
void VoxDecor_Unload(void);

#endif
