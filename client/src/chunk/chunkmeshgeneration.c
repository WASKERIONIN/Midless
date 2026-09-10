/**
 * Copyright (c) 2022 Sirvoid
 * This software is released under the MIT License.
 */

#include <stddef.h>
#include "raylib.h"
#include "chunkmeshgeneration.h"
#include "blockmeshgeneration.h"

int chunkTriangleCount = 0;
int chunkTransparentTriangleCount = 0;

static unsigned char *vertices, *colors, *verticesT, *colorsT;
static unsigned short *indices, *texcoords, *indicesT, *texcoordsT;

void ChunkMeshGeneration_Init(void) {
    int vertexCount = 2 * 6 * CHUNK_SIZE * 2;
    int triangleCount = 2 * 6 * CHUNK_SIZE;
    vertices = MemAlloc(vertexCount * 3);
    texcoords = MemAlloc(vertexCount * 2 * sizeof(unsigned short));
    colors = MemAlloc(vertexCount);
    indices = MemAlloc(triangleCount * 3 * sizeof(unsigned short));
    verticesT = MemAlloc(vertexCount * 3);
    texcoordsT = MemAlloc(vertexCount * 2 * sizeof(unsigned short));
    colorsT = MemAlloc(vertexCount);
    indicesT = MemAlloc(triangleCount * 3 * sizeof(unsigned short));
}

void ChunkMeshGeneration_Shutdown(void) {
    MemFree(vertices);
    MemFree(texcoords);
    MemFree(colors);
    MemFree(indices);
    MemFree(verticesT);
    MemFree(texcoordsT);
    MemFree(colorsT);
    MemFree(indicesT);

    vertices = NULL;
    texcoords = NULL;
    colors = NULL;
    indices = NULL;
    verticesT = NULL;
    texcoordsT = NULL;
    colorsT = NULL;
    indicesT = NULL;
}

static bool SameLiquidOccludes(const Block *block, const Block *next) {
    return block == next && block->colliderType == BLOCK_COLLIDER_LIQUID &&
           block->modelType == BLOCK_MODEL_SOLID && block->fullCube;
}

static bool FaceVisible(const Block *block, const Block *next) {
    if (SameLiquidOccludes(block, next)) return false;
    if (next->colliderType != BLOCK_COLLIDER_SOLID) return true;
    if (block->fastOpaqueCube) {
        return !next->fastOpaqueCube;
    }
    if (block->renderType == BLOCK_RENDER_OPAQUE) return ChunkMeshGeneration_IsOpaqueFaceVisible(block, next);
    if (block->renderType == BLOCK_RENDER_TRANSLUCENT) return ChunkMeshGeneration_IsTranslucentFaceVisible(block, next);
    return true;
}

/* ----------------------------------------------------------------------- *
 * v43: per-vertex ambient occlusion. For each face corner we sample the
 * two edge neighbours and the diagonal in the layer the face looks into;
 * occluded corners get darker light nibbles baked into the vertex colors.
 * ----------------------------------------------------------------------- */

/* 26-neighbourhood offsets, matching Chunk_UpdateNeighbours(). */
static const Vector3 kNeighbourDirs[26] = {
    {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
    {-1, -1, -1}, {1, 1, 1}, {-1, -1, 0}, {1, 1, 0}, {-1, -1, 1}, {1, 1, -1},
    {-1, 0, -1}, {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 1, -1}, {1, -1, 1},
    {-1, 1, 0}, {1, -1, 0}, {-1, 1, 1}, {1, -1, -1}, {0, -1, -1}, {0, 1, 1},
    {0, -1, 1}, {0, 1, -1}
};

static bool SampleOccludes(Chunk *chunk, int x, int y, int z) {
    int dx = 0, dy = 0, dz = 0;
    if (x < 0) { dx = -1; x += CHUNK_SIZE_X; } else if (x >= CHUNK_SIZE_X) { dx = 1; x -= CHUNK_SIZE_X; }
    if (y < 0) { dy = -1; y += CHUNK_SIZE_Y; } else if (y >= CHUNK_SIZE_Y) { dy = 1; y -= CHUNK_SIZE_Y; }
    if (z < 0) { dz = -1; z += CHUNK_SIZE_Z; } else if (z >= CHUNK_SIZE_Z) { dz = 1; z -= CHUNK_SIZE_Z; }

    Chunk *target = chunk;
    if (dx | dy | dz) {
        for (int i = 0; i < 26; i++) {
            if ((int)kNeighbourDirs[i].x == dx && (int)kNeighbourDirs[i].y == dy &&
                (int)kNeighbourDirs[i].z == dz) {
                target = chunk->neighbours[i];
                break;
            }
        }
        if (target == NULL) return false;
    }
    if ((unsigned)x >= CHUNK_SIZE_X || (unsigned)y >= CHUNK_SIZE_Y || (unsigned)z >= CHUNK_SIZE_Z)
        return false;
    int index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
    return blockDefinitions[target->data[index]].fastOpaqueCube;
}

/* Per-corner tangent signs come from the template box coordinates. */
static void ComputeFaceAO(Chunk *chunk, int nx, int ny, int nz, BlockFace face,
                          const unsigned char source[12], unsigned char ao[4]) {
    static const int axis1[6] = {2, 2, 0, 0, 0, 0}; /* 0=x 1=y 2=z */
    static const int axis2[6] = {1, 1, 2, 2, 1, 1};
    int a1 = axis1[face], a2 = axis2[face];
    int base[3] = { nx, ny, nz };

    for (int corner = 0; corner < 4; corner++) {
        int s1 = source[corner * 3 + a1] > 8 ? 1 : -1;
        int s2 = source[corner * 3 + a2] > 8 ? 1 : -1;

        int p1[3] = { base[0], base[1], base[2] };
        p1[a1] += s1;
        int p2[3] = { base[0], base[1], base[2] };
        p2[a2] += s2;
        int pc[3] = { base[0], base[1], base[2] };
        pc[a1] += s1;
        pc[a2] += s2;

        bool side1 = SampleOccludes(chunk, p1[0], p1[1], p1[2]);
        bool side2 = SampleOccludes(chunk, p2[0], p2[1], p2[2]);
        bool diag = SampleOccludes(chunk, pc[0], pc[1], pc[2]);

        int level = (side1 && side2) ? 0 : 3 - ((side1 ? 1 : 0) + (side2 ? 1 : 0) + (diag ? 1 : 0));
        static const int shade[4] = {6, 9, 12, 15}; /* 0..15 multiplier */
        ao[corner] = (unsigned char)shade[level];
    }
}

static void AddFace(Chunk *chunk, int blockIndex, int x, int y, int z,
                    BlockFace face, const Block *block) {
    static const int indexOffsets[6] = {-1, 1, CHUNK_SIZE_XZ, -CHUNK_SIZE_XZ, CHUNK_SIZE_X, -CHUNK_SIZE_X};
    int nox = 0, noy = 0, noz = 0;
    int nx = x, ny = y, nz = z;
    if (face == BLOCK_FACE_LEFT) { nx--; nox = -1; }
    else if (face == BLOCK_FACE_RIGHT) { nx++; nox = 1; }
    else if (face == BLOCK_FACE_TOP) { ny++; noy = 1; }
    else if (face == BLOCK_FACE_BOTTOM) { ny--; noy = -1; }
    else if (face == BLOCK_FACE_FRONT) { nz++; noz = 1; }
    else { nz--; noz = -1; }

    Chunk *nextChunk = chunk;
    int nextIndex;
    if ((unsigned)nx < CHUNK_SIZE_X && (unsigned)ny < CHUNK_SIZE_Y && (unsigned)nz < CHUNK_SIZE_Z) {
        nextIndex = blockIndex + indexOffsets[(int)face];
    } else {
        nextChunk = chunk->neighbours[(int)face];
        if (nextChunk == NULL) return;
        if (nx < 0) nx = CHUNK_SIZE_X - 1; else if (nx == CHUNK_SIZE_X) nx = 0;
        if (ny < 0) ny = CHUNK_SIZE_Y - 1; else if (ny == CHUNK_SIZE_Y) ny = 0;
        if (nz < 0) nz = CHUNK_SIZE_Z - 1; else if (nz == CHUNK_SIZE_Z) nz = 0;
        nextIndex = (ny * CHUNK_SIZE_Z + nz) * CHUNK_SIZE_X + nx;
    }

    const Block *next = &blockDefinitions[nextChunk->data[nextIndex]];
    bool sprite = block->modelType == BLOCK_MODEL_SPRITE;
    if (!sprite && !FaceVisible(block, next)) return;

    int light;
    int sunlight;
    if (sprite || block->renderType == BLOCK_RENDER_TRANSPARENT) {
        light = chunk->lightData[blockIndex];
        sunlight = chunk->sunlightData[blockIndex];
    } else if (!block->fullCube) {
        int ownLight = chunk->lightData[blockIndex];
        int ownSunlight = chunk->sunlightData[blockIndex];
        int neighborLight = nextChunk->lightData[nextIndex];
        int neighborSunlight = nextChunk->sunlightData[nextIndex];
        light = ownLight > neighborLight ? ownLight : neighborLight;
        sunlight = ownSunlight > neighborSunlight ? ownSunlight : neighborSunlight;
    } else {
        light = nextChunk->lightData[nextIndex];
        sunlight = nextChunk->sunlightData[nextIndex];
    }

    /* v43: ambient occlusion for every non-sprite face */
    unsigned char ao[4] = {15, 15, 15, 15};
    if (!sprite) {
        const BlockMeshTemplate *meshTemplate = BlockMesh_GetTemplate((int)(block - blockDefinitions));
        if (meshTemplate != NULL)
            ComputeFaceAO(chunk, x + nox, y + noy, z + noz, face, meshTemplate->vertices[(int)face], ao);
    }

    if (block->renderType == BLOCK_RENDER_TRANSLUCENT) {
        chunkTransparentTriangleCount += 2;
        BlockMesh_AddFace(verticesT, indicesT, texcoordsT, colorsT, face, x, y, z, block, 1, light, sunlight, ao);
    } else {
        chunkTriangleCount += 2;
        BlockMesh_AddFace(vertices, indices, texcoords, colors, face, x, y, z, block, 0, light, sunlight, ao);
    }
}

void ChunkMeshGeneration_Build(Chunk *chunk) {
    BlockMesh_ResetIndexes();
    chunkTriangleCount = 0;
    chunkTransparentTriangleCount = 0;
    chunk->hasTransparency = false;
    chunk->onlyAir = true;
    chunk->specialCount[0] = 0;
    chunk->specialCount[1] = 0;

    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X;
            for (int x = 0; x < CHUNK_SIZE_X; x++, index++) {
                unsigned int blockId = chunk->data[index];
                const Block *block = &blockDefinitions[blockId];
                if (block->modelType == BLOCK_MODEL_GAS) continue;
                chunk->onlyAir = false;
                /* v44: track special blocks for the wireframe aura pass */
                if (blockId == 22 || blockId == 21) {
                    int slot = (blockId == 22) ? 0 : 1;
                    if (chunk->specialCount[slot] < 24) {
                        chunk->specialPos[slot][chunk->specialCount[slot]++] =
                            (Vector3){ chunk->blockPosition.x + x + 0.5f,
                                       chunk->blockPosition.y + y + 0.5f,
                                       chunk->blockPosition.z + z + 0.5f };
                    }
                }
                if (block->renderType == BLOCK_RENDER_TRANSLUCENT) chunk->hasTransparency = true;
                int faceCount = block->modelType == BLOCK_MODEL_SPRITE ? 4 : 6;
                for (int face = 0; face < faceCount; face++) AddFace(chunk, index, x, y, z, (BlockFace)face, block);
            }
        }
    }

    chunk->mesh.vertexCount = chunkTriangleCount * 2;
    chunk->mesh.triangleCount = chunkTriangleCount;
    chunk->meshTransparent.vertexCount = chunkTransparentTriangleCount * 2;
    chunk->meshTransparent.triangleCount = chunkTransparentTriangleCount;

    if (chunk->mesh.triangleCount > 0) ChunkMesh_Upload(&chunk->mesh, vertices, indices, texcoords, colors);
    else ChunkMesh_Clear(&chunk->mesh);
    if (chunk->meshTransparent.triangleCount > 0) ChunkMesh_Upload(&chunk->meshTransparent, verticesT, indicesT, texcoordsT, colorsT);
    else ChunkMesh_Clear(&chunk->meshTransparent);
    chunk->isBuilt = true;
    chunk->isLightDirty = false;
}

bool ChunkMeshGeneration_IsOpaqueFaceVisible(const Block *block, const Block *next) {
    if (SameLiquidOccludes(block, next)) return false;
    if (next->colliderType != BLOCK_COLLIDER_SOLID) return true;
    if (next->modelType == BLOCK_MODEL_GAS) return true;
    if (next->renderType != BLOCK_RENDER_OPAQUE) return true;
    if (next->modelType == BLOCK_MODEL_SPRITE) return true;
    return !block->fullCube || !next->fullCube;
}

bool ChunkMeshGeneration_IsTranslucentFaceVisible(const Block *block, const Block *next) {
    if (SameLiquidOccludes(block, next)) return false;
    if (next->colliderType != BLOCK_COLLIDER_SOLID) return true;
    if (next->modelType == BLOCK_MODEL_GAS) return true;
    /* v43.5: full transparent neighbours (glass, crystal) cull the water face
     * just like opaque ones - drawing both left coplanar faces that z-fought
     * (texture shimmer that read as "inversion" when moving). */
    return !block->fullCube || !next->fullCube;
}
