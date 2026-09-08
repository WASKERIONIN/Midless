#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "chunk/chunk.h"
#include "worldgenerator.h"
int main(int argc, char **argv) {
    int seed = atoi(argv[1]); int cx = atoi(argv[2]), cy = atoi(argv[3]), cz = atoi(argv[4]); int st = argc > 5 ? atoi(argv[5]) : 1;
    ServerWorldGenerator_Init(seed);
    Chunk *c = MemAlloc(sizeof *c);
    c->position = (Vector3){cx, cy, cz};
    c->blockPosition = Vector3Multiply(c->position, CHUNK_SIZE_VEC3);
    ServerWorldGenerator_Generate(c);
    if (st) ServerWorldGenerator_GenerateStructures(c);
    fwrite(c->data, 2, CHUNK_SIZE, stdout);
    return 0;
}
