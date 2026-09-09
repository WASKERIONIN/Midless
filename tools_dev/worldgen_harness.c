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

        if (!okStarter) { printf("STARTER ISLAND INCOMPLETE\n"); ok = false; }
    }

    ServerNetwork_Shutdown();
    ServerWorld_Shutdown();
    LuaBindings_Shutdown();
    return ok ? 0 : 1;
}
