#ifndef MIDLESS_CLIENT_RAINCLOUD_H
#define MIDLESS_CLIENT_RAINCLOUD_H

#include "raylib.h"

/* v63.7: wandering rain clouds. When a cloud hovers over land it rains,
 * and mushrooms sprout where the drops land - only the biome's own
 * species (glowcap cluster on classic turf, cinder trumpet on ember
 * turf, frost puffball on frost turf). Mushrooms appear ONLY this way. */

void RainCloud_Init(void);
void RainCloud_Shutdown(void);
void RainCloud_Update(float dt);
void RainCloud_Draw(void);

#endif
