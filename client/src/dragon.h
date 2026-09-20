#ifndef MIDLESS_DRAGON_H
#define MIDLESS_DRAGON_H
/* v65.41: the wandering dragon. Every few minutes a dragon (ephtracy's
 * sample scan model, models/vox/dragon.vox, sub-block voxels) appears
 * over a random island near the player and circles it for a while.
 * Its arrival GILDS the island: some natural surface blocks turn into
 * Gold Plate (24). Any gilded block not mined within 10 minutes turns
 * back into the original block. Pending restorations survive a restart
 * (dragonhoard.dat next to the executable). Main world only - the
 * pockets never see it. */
#include "raylib.h"

void Dragon_Update(void);   /* call per frame inside the main-world gate */
/* v65.42: while the dragon hovers, the mushroom rain cloud keeps away */
bool Dragon_GetAnchor(Vector3 *out);
void Dragon_Draw(Vector3 camPos);
void Dragon_Unload(void);

#endif
