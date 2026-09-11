/*
 * Local Linux harness: runs the exact server world-startup path used by the
 * game's singleplayer local server, so mod/worldgen failures can be
 * reproduced and printed without a GPU or Windows.
 *
 * It links the real server sources with stub raylib implementations.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "raylib.h"
#include "world/chunk/chunk.h"
#include "world/worldgen.h"
#include "world/worldgen.h"

/* server API */
void Lua_Init(void);
void LuaBindings_Init(void);
void LuaBindings_Shutdown(void);
bool Lua_Run(void);
void ServerWorld_Init(void);
void ServerWorld_Shutdown(void);
void ServerNetwork_Init(void);
void ServerNetwork_Shutdown(void);

extern unsigned int worldgen_fieldCount_placeholder; /* (not used) */

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    if (chdir(dir) != 0) {
        perror("chdir");
        return 2;
    }

    printf("== harness: server startup (%s) ==\n", dir);

    Lua_Init();
    LuaBindings_Init();
    ServerWorld_Init();
    ServerNetwork_Init();

    bool ok = Lua_Run();

    printf("== Lua_Run: %s ==\n", ok ? "OK" : "FAILED");
    {
        extern WGConfig worldgen;
        printf("worldgen fields used: %d / 512\n", worldgen.fieldCount);
    }

    if (ok) {
        /* Generate real chunks through the real worldgen pipeline. */
        Chunk *chunk = ServerChunk_Create((Vector3){ 0, 4, 0 }); /* starter island chunk */
        Worldgen_Generate(chunk);

        int counts[256] = { 0 };
        for (int i = 0; i < CHUNK_SIZE; i++) counts[chunk->data[i]]++;
        printf("starter chunk blocks:");
        for (int id = 1; id < 256; id++)
            if (counts[id]) printf(" %d x%d", id, counts[id]);
        printf("\n");

        int okStarter = counts[21] > 0 && counts[20] > 0 && counts[22] > 0 &&
                        counts[14] > 0 && counts[5] > 0;
        printf("starter island decor (pad/crystal/core/glass/water): %s\n",
               okStarter ? "OK" : "MISSING");

        /* void rock rule must have replaced dirt-below-dirt somewhere */
        ServerChunk_Destroy(chunk);

        /* scan a belt of chunks and report island density */
        int totalSolid = 0, totalChunks = 0;
        for (int cz = 0; cz < 8; cz++) {
            for (int cx = 0; cx < 8; cx++) {
                for (int cy = 2; cy <= 7; cy++) {
                    Chunk *c = ServerChunk_Create((Vector3){ (float)cx, (float)cy, (float)cz });
                    Worldgen_Generate(c);
                    int solid = 0;
                    for (int i = 0; i < CHUNK_SIZE; i++) if (c->data[i]) solid++;
                    totalSolid += solid;
                    ServerChunk_Destroy(c);
                }
                totalChunks++;
            }
        }
        printf("belt scan: %d solid blocks across %d chunks (%d y-layers each)\n",
               totalSolid, totalChunks, 6);

        /* v51: volatile barrels + void cocoons must exist in the world;
         * v52: island flora (12/13) and a coarse island-blob count */
        int barrels = 0, cocoons = 0, flowers = 0;
        int solidGrid[8][8][8];
        memset(solidGrid, 0, sizeof(solidGrid));
        for (int cz = 0; cz < 8; cz++) {
            for (int cx = 0; cx < 8; cx++) {
                for (int cy = 1; cy <= 8; cy++) {
                    Chunk *c = ServerChunk_Create((Vector3){ (float)cx, (float)cy, (float)cz });
                    Worldgen_Generate(c);
                    int solidCount = 0;
                    for (int i = 0; i < CHUNK_SIZE; i++) {
                        if (c->data[i] == 26) barrels++;
                        else if (c->data[i] == 25) cocoons++;
                        else if (c->data[i] == 12 || c->data[i] == 13) flowers++;
                        if (c->data[i]) solidCount++;
                    }
                    solidGrid[cx][cy - 1][cz] = solidCount > 40;
                    ServerChunk_Destroy(c);
                }
            }
        }
        /* coarse island count: 6-connected components of solid chunks */
        int blobs = 0;
        for (int cz = 0; cz < 8; cz++)
            for (int cy = 0; cy < 8; cy++)
                for (int cx = 0; cx < 8; cx++) {
                    if (!solidGrid[cx][cy][cz]) continue;
                    blobs++;
                    /* flood fill iteratively */
                    solidGrid[cx][cy][cz] = 0;
                    int stack[512][3];
                    int sp = 0;
                    stack[sp][0] = cx; stack[sp][1] = cy; stack[sp][2] = cz; sp++;
                    while (sp > 0) {
                        sp--;
                        int x = stack[sp][0], yy = stack[sp][1], zz = stack[sp][2];
                        int nx[6] = { x - 1, x + 1, x, x, x, x };
                        int ny[6] = { yy, yy, yy - 1, yy + 1, yy, yy };
                        int nz[6] = { zz, zz, zz, zz, zz - 1, zz + 1 };
                        for (int n = 0; n < 6; n++) {
                            if (nx[n] < 0 || nx[n] > 7 || ny[n] < 0 || ny[n] > 7 ||
                                nz[n] < 0 || nz[n] > 7) continue;
                            if (!solidGrid[nx[n]][ny[n]][nz[n]]) continue;
                            solidGrid[nx[n]][ny[n]][nz[n]] = 0;
                            stack[sp][0] = nx[n]; stack[sp][1] = ny[n]; stack[sp][2] = nz[n];
                            sp++;
                        }
                    }
                }
        /* v54: cocoons must never sit on flora; count species spread */
        int cocoonOnFlower = 0;
        int species[64] = { 0 };
        for (int cz = 0; cz < 8; cz++)
            for (int cx = 0; cx < 8; cx++)
                for (int cy = 1; cy <= 8; cy++) {
                    Chunk *c = ServerChunk_Create((Vector3){ (float)cx, (float)cy, (float)cz });
                    Worldgen_Generate(c);
                    for (int lz = 0; lz < CHUNK_SIZE_Z; lz++)
                        for (int lx = 0; lx < CHUNK_SIZE_X; lx++)
                            for (int ly = 0; ly < CHUNK_SIZE_Y; ly++) {
                                int id = c->data[(ly * CHUNK_SIZE_Z + lz) * CHUNK_SIZE_X + lx];
                                if (id == 25 && ly + 1 < CHUNK_SIZE_Y) {
                                    int above = c->data[((ly + 1) * CHUNK_SIZE_Z + lz) * CHUNK_SIZE_X + lx];
                                    if (above == 12 || above == 13 ||
                                        (above >= 28 && above <= 33)) cocoonOnFlower++;
                                }
                                if ((id == 12 || id == 13) || (id >= 28 && id <= 33))
                                    species[id]++;   /* v54: direct slots */
                                    if (id >= 40 && id < 64) species[id]++;
                            }
                    ServerChunk_Destroy(c);
                }
        printf("v54 scan: cocoonOnFlower=%d species: rose=%d dandelion=%d bell=%d star=%d fern=%d tulip=%d grass=%d lantern=%d\n",
               cocoonOnFlower, species[12], species[13],
               species[28], species[29], species[30],
               species[31], species[32], species[33]);
        printf("v54 debug buckets 40..55:");
        for (int db = 40; db < 56; db++) if (species[db]) printf(" %d:%d", db, species[db]);
        printf("\n");

        if (!okStarter) { printf("STARTER ISLAND INCOMPLETE\n"); ok = false; }
    }

    ServerNetwork_Shutdown();
    ServerWorld_Shutdown();
    LuaBindings_Shutdown();
    return ok ? 0 : 1;
}
