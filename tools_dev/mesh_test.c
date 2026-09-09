/**
 * Headless unit test for BlockMesh_AddFace buffer layout invariants.
 * White-box: includes the .c directly to read the static stream counters.
 * Catches vertex/texcoord/color/index counter desync (v43.3 regression guard).
 */
#include "blockmeshgeneration.c"
#include <stdio.h>

/* test-local definition: block.c drags in renderer deps we don't need */
Block blockDefinitions[256];

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

int main(void) {
    static unsigned char vertices[4096];
    static unsigned short indices[4096];
    static unsigned short texcoords[4096];
    static unsigned char colors[4096];

    Block *stone = &blockDefinitions[1];
    memset(stone, 0, sizeof(Block));
    strcpy(stone->name, "stone");
    stone->modelType = BLOCK_MODEL_SOLID;
    stone->renderType = BLOCK_RENDER_OPAQUE;
    stone->fullCube = true;
    stone->minBB = (Vector3){0, 0, 0};
    stone->maxBB = (Vector3){16, 16, 16};
    BlockMesh_BuildTemplates();

    const Block *block = &blockDefinitions[1];
    unsigned char ao[4] = {15, 12, 7, 10};

    BlockMesh_ResetIndexes();
    BlockMesh_AddFace(vertices, indices, texcoords, colors, BLOCK_FACE_TOP, 0, 0, 0,
                      block, 0, 15, 15, ao);

    CHECK(verticesIndex[0] == 12, "one face writes 12 vertex bytes (4 verts x xyz)");
    CHECK(colorsIndex[0] == 4, "one face writes 4 vertex colors");
    CHECK(textureIndex[0] == 8, "one face writes 8 texcoord ushorts");
    CHECK(indicesIndex[0] == 6, "one face writes 6 indices");

    bool idxValid = true;
    for (int i = 0; i < 6; i++)
        if (indices[i] >= 4) idxValid = false;
    CHECK(idxValid, "all indices reference corners 0..3 of this face");

    bool distinct = (colors[0] != colors[2]) || (colors[1] != colors[3]);
    CHECK(distinct, "per-vertex colors differ under non-uniform AO");

    BlockMesh_AddFace(vertices, indices, texcoords, colors, BLOCK_FACE_TOP, 1, 0, 0,
                      block, 0, 15, 15, ao);
    CHECK(verticesIndex[0] == 24, "two faces -> 24 vertex bytes");
    CHECK(indicesIndex[0] == 12, "two faces -> 12 indices");
    bool secondFaceBase = (indices[6] >= 4) && (indices[6] < 8);
    CHECK(secondFaceBase, "second face indices base at vertex 4");

    BlockMesh_ResetIndexes();
    BlockMesh_AddFace(vertices, indices, texcoords, colors, BLOCK_FACE_TOP, 0, 0, 0,
                      block, 1, 15, 15, ao);
    CHECK(verticesIndex[1] == 12 && indicesIndex[1] == 6 &&
          textureIndex[1] == 8 && colorsIndex[1] == 4,
          "translucent stream counters independent");

    Block *flower = &blockDefinitions[2];
    memset(flower, 0, sizeof(Block));
    strcpy(flower->name, "flower");
    flower->modelType = BLOCK_MODEL_SPRITE;
    flower->renderType = BLOCK_RENDER_OPAQUE;
    BlockMesh_BuildTemplates();
    BlockMesh_ResetIndexes();
    unsigned char aoFull[4] = {15, 15, 15, 15};
    BlockMesh_AddFace(vertices, indices, texcoords, colors, BLOCK_FACE_TOP, 0, 0, 0,
                      &blockDefinitions[2], 0, 15, 15, aoFull);
    CHECK(verticesIndex[0] == 12 && colorsIndex[0] == 4 && textureIndex[0] == 8,
          "sprite face keeps layout invariants");

    if (failures == 0) printf("\nMESH TEST OK\n");
    else printf("\nMESH TEST FAILED (%d)\n", failures);
    return failures != 0;
}
