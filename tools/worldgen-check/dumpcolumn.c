#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "raylib.h"
#include "minilua.h"
#include "world.h"
#include "worldgen.h"
#include "worldgenerator.h"
#include "chunk/chunk.h"
extern lua_State *L;
void LuaWorldgen_Init(void);
static void MakeChunk(Chunk *c, int cx, int cy, int cz) {
    memset(c, 0, sizeof(*c));
    c->position = (Vector3){cx, cy, cz};
    c->blockPosition = (Vector3){cx * CHUNK_SIZE_X, cy * CHUNK_SIZE_Y, cz * CHUNK_SIZE_Z};
}
int main(int argc, char **argv) {
    int wx = atoi(argv[2]), wz = atoi(argv[3]);
    mkdir("world", 0755);
    ServerWorldGenerator_Init(1337);
    L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    lua_setglobal(L, "midless");
    LuaWorldgen_Init();
    if (luaL_dofile(L, argv[1]) != 0) { printf("lua: %s\n", lua_tostring(L, -1)); return 1; }
    if (!Worldgen_Freeze()) return 1;
    int cx = (int)floorf(wx / 16.0f), cz = (int)floorf(wz / 16.0f);
    int lx = wx - cx * 16, lz = wz - cz * 16;
    printf("biome[0] depth=%d top=%d filler=%d stone=%d underwater=%d sea=%d\n",
           worldgen.biomes[0].depth, worldgen.biomes[0].top, worldgen.biomes[0].filler,
           worldgen.biomes[0].stone, worldgen.biomes[0].underwater, worldgen.seaLevel);
    printf("column x=%d z=%d (chunk %d,%d local %d,%d)\n", wx, wz, cx, cz, lx, lz);
    for (int cy = 5; cy >= 2; cy--) {
        static Chunk c;
        MakeChunk(&c, cx, cy, cz);
        Worldgen_Generate(&c);
        for (int y = 15; y >= 0; y--) {
            int wy = cy * 16 + y;
            int id = c.data[(y * CHUNK_SIZE_Z + lz) * CHUNK_SIZE_X + lx];
            printf("  y=%4d  id=%3d%s\n", wy, id, id ? "" : "  (air)");
        }
    }
    return 0;
}
