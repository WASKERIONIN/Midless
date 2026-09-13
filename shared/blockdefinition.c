#include <string.h>
#include "blockdefinition.h"

bool BlockDefinition_Validate(int id, const BlockDefinition *d) {
    if (id < 1 || id > 255 || !d || !d->name[0] ||
        !memchr(d->name, 0, sizeof(d->name))) return false;
    if (d->modelType > BLOCK_MODEL_SPRITE || d->renderType > BLOCK_RENDER_TRANSLUCENT ||
        d->colliderType > BLOCK_COLLIDER_LIQUID || d->lightType > BLOCK_LIGHT_EMIT) return false;
    /* v65.3: graded emission; a lightLevel without EMIT is a mod mistake */
    if (d->lightLevel > 15) return false;
    if (d->lightLevel > 0 && d->lightType != BLOCK_LIGHT_EMIT) return false;
    for (int i = 0; i < 3; i++) {
        if (d->min[i] >= d->max[i] || d->max[i] > 16) return false;
    }
    return true;
}
