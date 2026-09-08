// Print a vertical slice (X along, fixed Z) at world scale to show the wall visually.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"
#include "chunk/chunk.h"
#include "worldgenerator.h"
int main(int argc, char **argv) {
    int seed = atoi(argv[1]); int z = atoi(argv[2]); int x0 = atoi(argv[3]); int x1 = atoi(argv[4]);
    ServerWorldGenerator_Init(seed);
    Chunk *c = MemAlloc(sizeof *c);
    int cz = (z >= 0 ? z : z - 15) / 16; if (z < 0) cz = -((-z + 15) / 16);
    int cx0 = (x0 >= 0) ? x0/16 : -((-x0+15)/16), cx1 = (x1 >= 0) ? x1/16 : -((-x1+15)/16);
    int W = (cx1-cx0+1)*16; unsigned short *col = calloc((size_t)W*96, 2);
    for (int cy = 0; cy < 6; cy++) for (int cx = cx0; cx <= cx1; cx++) {
        memset(c, 0, sizeof *c); c->position = (Vector3){cx,cy,cz}; c->blockPosition = Vector3Multiply(c->position, CHUNK_SIZE_VEC3);
        ServerWorldGenerator_Generate(c); ServerWorldGenerator_GenerateStructures(c);
        int lz = z - cz*16;
        for (int ly = 0; ly < 16; ly++) for (int lx = 0; lx < 16; lx++) col[(cy*16+ly)*W + (cx-cx0)*16+lx] = c->data[(ly*16+lz)*16+lx];
    }
    for (int y = 60; y >= 30; y--) { printf("y=%2d ", y); for (int x = x0; x <= x1; x++) { int id = col[y*W + (x - cx0*16)]; putchar(id==0?'.':id==5?'~':id==6?'s':id==1?'#':id==3?'g':id==2?'d':id==20?'i':id==21?'S':id==19?'*':'o'); } printf("\n"); }
    return 0;
}
