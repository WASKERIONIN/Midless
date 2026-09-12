/* v62 mod probe: load mods/ through the real server Lua engine, then
 * evaluate the frozen material field over a wide grid and print stats.
 * Not part of the shipped game - throwaway validation tool. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "scripting/luaengine.h"
#include "world/worldgen.h"

void LuaBindings_Init(void);
void LuaBindings_Shutdown(void);

static const char *BlockName(unsigned short id) {
    static const char *names[] = {
        "air", "stone", "dirt", "grass", "wood", "water", "sand", "gravel",
        "gold", "gem", "root", "beam", "lantern", "basalt", "glass", "torch",
        "ice", "mossy", "loam", "voidrock", "shard", "bloom", "petal",
        "bell", "wick", "fern", "cap", "stem", "web", "cocoon", "eggs",
        "twinpetal", "glimmer", "lanternberry", "glowshroom", "snowtulip",
        "cobalt", "tree_lantern", "tree_void", "vine", "orb", "grass_tuft",
        "crystal_sprout", "starbloom", "hanging_vine", "glassbell",
        "embrcup(46)", "orchid(47)", "frostfern(48)", "tree_big_a", "cane",
        "tree_big_b", "void_puff", "ember_rock", "ember_turf",
        "frost_turf", "ember_tuft", "frost_tuft",
    };
    static char buf[32];
    if (id < sizeof(names) / sizeof(names[0])) return names[id];
    snprintf(buf, sizeof(buf), "id%d", id);
    return buf;
}

int main(void) {
    Lua_Init();
    LuaBindings_Init();
    if (!Lua_Run()) {
        printf("PROBE: LUA RUN FAILED\n");
        return 1;
    }
    printf("PROBE: mod loaded OK; fields=%d material=%d features=%d\n",
           worldgen.fieldCount, worldgen.material, worldgen.featureCount);
    if (worldgen.material < 0) { printf("PROBE: no material field\n"); return 1; }

    long counts[256]; memset(counts, 0, sizeof(counts));
    long surfaceCounts[256]; memset(surfaceCounts, 0, sizeof(surfaceCounts));
    long cells = 0, islandsCells = 0;
    int topY;
    /* wide scan: x,z in [-512, 512] step 8, full height step 2 */
    for (int x = -512; x <= 512; x += 8) {
        for (int z = -512; z <= 512; z += 8) {
            WGEval ctx;
            Worldgen_EvalInit(&ctx, (Vector3){x, 0, z}, (Vector3){x, 0, z});
            int colTop = -1; unsigned short colTopId = 0;
            for (int y = 0; y <= 140; y += 1) {
                Worldgen_EvalY(&ctx, y);
                float id = worldgen.bounded && (y < worldgen.minY || y > worldgen.maxY)
                               ? 0 : Worldgen_Eval(&ctx, worldgen.material);
                unsigned short b = id >= 1 && id <= 255 ? (int)id : 0;
                if (b) {
                    counts[b]++;
                    islandsCells++;
                    if (colTop < 0 || y > colTop) { colTop = y; colTopId = b; }
                }
                cells++;
            }
            if (colTop >= 0) { surfaceCounts[colTopId]++; topY = colTop; (void)topY; }
        }
    }
    printf("PROBE: cells=%ld solidCells=%ld\n", cells, islandsCells);
    printf("PROBE: block histogram (nonzero):\n");
    for (int i = 1; i < 256; i++)
        if (counts[i]) printf("  %3d %-16s %ld\n", i, BlockName(i), counts[i]);
    printf("PROBE: top-of-column block histogram (island count proxy, 129x129 grid):\n");
    for (int i = 1; i < 256; i++)
        if (surfaceCounts[i]) printf("  %3d %-16s %ld\n", i, BlockName(i), surfaceCounts[i]);
    /* spatial map: dominant top-class per 16x16 cell over +-512 */
    printf("PROBE: spatial map (G=classic E=ember F=frost .=void):\n");
    for (int gz = -512; gz < 512; gz += 16) {
        printf("  ");
        for (int gx = -512; gx < 512; gx += 16) {
            long ng = 0, ne = 0, nf = 0;
            for (int x = gx; x < gx + 16; x += 4) {
                for (int z = gz; z < gz + 16; z += 4) {
                    WGEval ctx;
                    Worldgen_EvalInit(&ctx, (Vector3){x, 0, z}, (Vector3){x, 0, z});
                    int top = -1; unsigned short tid = 0;
                    for (int y = 0; y <= 140; y++) {
                        Worldgen_EvalY(&ctx, y);
                        float id = worldgen.bounded && (y < worldgen.minY || y > worldgen.maxY)
                                       ? 0 : Worldgen_Eval(&ctx, worldgen.material);
                        unsigned short b = id >= 1 && id <= 255 ? (int)id : 0;
                        if (b) { top = y; tid = b; }
                    }
                    if (tid == 57 || tid == 56) ne++;
                    else if (tid == 58) nf++;
                    else if (tid) ng++;
                }
            }
            long tot = ng + ne + nf;
            if (tot == 0) { printf("."); continue; }
            if (ne * 2 > tot) printf("E");
            else if (nf * 2 > tot) printf("F");
            else printf("G");
        }
        printf("\n");
    }
    /* starter island check: all tops within |x-8|<11,|z-8|<11 must be classic */
    int bad = 0;
    for (int x = -2; x <= 18; x++)
        for (int z = -2; z <= 18; z++) {
            WGEval ctx;
            Worldgen_EvalInit(&ctx, (Vector3){x, 0, z}, (Vector3){x, 0, z});
            for (int y = 0; y <= 140; y++) {
                Worldgen_EvalY(&ctx, y);
                float id = worldgen.bounded && (y < worldgen.minY || y > worldgen.maxY)
                               ? 0 : Worldgen_Eval(&ctx, worldgen.material);
                unsigned short b = id >= 1 && id <= 255 ? (int)id : 0;
                if (b == 56 || b == 57 || b == 58) bad++;
            }
        }
    printf("PROBE: starter-island biome-block cells (must be 0): %d\n", bad);
    printf("PROBE: OK\n");
    return 0;
}
