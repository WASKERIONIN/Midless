#ifndef ISLEFORGE_BLOCK_DEFINITION_H
#define ISLEFORGE_BLOCK_DEFINITION_H

#include <stdbool.h>
#include <stdint.h>

#define GAME_PROTOCOL_VERSION 7
#define PACKET_DEFINE_BLOCK 12
#define PACKET_REMOVE_BLOCK_DEFINITION 13
/* v65.8.1: +1 byte for lightLevel (graded emission was server-only) */
#define DEFINE_BLOCK_PACKET_SIZE 83
#define BLOCK_DEFAULT_LAST_ID 18

typedef enum BlockModelType {
    BLOCK_MODEL_GAS, BLOCK_MODEL_SOLID, BLOCK_MODEL_SPRITE
} BlockModelType;
typedef enum BlockLightType {
    BLOCK_LIGHT_NONE, BLOCK_LIGHT_EMIT
} BlockLightType;
typedef enum BlockRenderType {
    BLOCK_RENDER_OPAQUE, BLOCK_RENDER_TRANSPARENT, BLOCK_RENDER_TRANSLUCENT
} BlockRenderType;
typedef enum BlockColliderType {
    BLOCK_COLLIDER_NONE, BLOCK_COLLIDER_SOLID, BLOCK_COLLIDER_LIQUID
} BlockColliderType;

typedef struct BlockDefinition {
    char name[65];
    uint8_t textures[6];
    uint8_t modelType, renderType, colliderType, lightType;
    /* v65.3: graded emission strength (1..15) for lightType EMIT blocks.
     * 0 on an EMIT block means "full 15" (legacy fire/lava/warp core);
     * soft glows (mushrooms, blooms, crystal) use 3..7 so a whole meadow
     * of them reads as bioluminescence instead of a field of torches. */
    uint8_t lightLevel;
    uint8_t min[3], max[3];
} BlockDefinition;

bool BlockDefinition_Validate(int id, const BlockDefinition *definition);

#endif
