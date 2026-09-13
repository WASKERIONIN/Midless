/* v62 mod probe: load mods/ through the real server Lua engine, then
 * evaluate the frozen material field over a wide grid and print stats.
 * Not part of the shipped game - throwaway validation tool. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "scripting/luaengine.h"
#include "world/worldgen.h"
#include "world.h"

extern World serverWorld;

void LuaBindings_Init(void);
void LuaBindings_Shutdown(void);

static const char *BlockName(unsigned short id) {
    /* v63.7: sparse map of REAL block ids (the old dense list was
     * shifted by several entries and mislabeled every histogram) */
    static const char *names[80] = {
        [1] = "stone", [2] = "dirt", [3] = "turf", [19] = "voidrock",
        [12] = "rose", [13] = "dandelion",
        [28] = "bellflower", [29] = "starbloom", [30] = "spiral_fern",
        [31] = "twin_tulip", [32] = "glow_grass", [33] = "lanternberry",
        [37] = "star_reed", [38] = "moon_bell", [39] = "void_tuft",
        [41] = "void_sedge", [45] = "glassbell", [46] = "embercup",
        [47] = "void_orchid", [48] = "frostfern", [49] = "void_tree",
        [50] = "lantern_tree", [51] = "crystal_stalk", [52] = "void_puff",
        [56] = "ember_rock", [57] = "ember_turf", [58] = "frost_turf",
        [59] = "ember_tuft", [60] = "frost_tuft",
        [67] = "smolderhead", [68] = "cinder_buds", [69] = "tree_lantern_low",
        [70] = "tree_void_low", [71] = "glacier_dewdrop", [72] = "ringbloom",
        [76] = "ember_lantern", [77] = "frost_burst",
        [73] = "glowcap_cluster", [74] = "cinder_trumpet",
        [75] = "frost_puffball",
    };
    static char buf[32];
    if (id < sizeof(names) / sizeof(names[0]) && names[id]) return names[id];
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
    /* v63b: stacked flora scan - any y where both y and y+1 are flora */
    {
        long stacks = 0;
        for (int x = -256; x <= 256; x += 4) {
            for (int z = -256; z <= 256; z += 4) {
                WGEval ctx;
                Worldgen_EvalInit(&ctx, (Vector3){x, 0, z}, (Vector3){x, 0, z});
                unsigned short prev = 0;
                for (int y = 0; y <= 140; y++) {
                    Worldgen_EvalY(&ctx, y);
                    float id = worldgen.bounded && (y < worldgen.minY || y > worldgen.maxY)
                                   ? 0 : Worldgen_Eval(&ctx, worldgen.material);
                    unsigned short b = id >= 1 && id <= 255 ? (int)id : 0;
                    int floraPrev = prev >= 12 && prev <= 72 && prev != 19 && prev != 20 &&
                                    prev != 21 && prev != 23 && prev != 26 && prev != 56 && prev != 57 &&
                                    prev != 58 && prev != 61 && prev != 62 && prev != 63;
                    int floraCur = b >= 12 && b <= 72 && b != 19 && b != 20 &&
                                   b != 21 && b != 23 && b != 26 && b != 56 && b != 57 &&
                                   b != 58 && b != 61 && b != 62 && b != 63;
                    if (floraPrev && floraCur) {
                        if (stacks < 8)
                            printf("PROBE: STACK at (%d,%d,%d): %d over %d\n", x, y, z, b, prev);
                        stacks++;
                    }
                    prev = b;
                }
            }
        }
        printf("PROBE: stacked-flora pairs: %ld\n", stacks);
    }
    /* debug column dump */
    {
        int xs[2] = {-236, -232};
        int zs[2] = {80, 96};
        for (int c = 0; c < 2; c++) {
            WGEval ctx;
            Worldgen_EvalInit(&ctx, (Vector3){xs[c], 0, zs[c]}, (Vector3){xs[c], 0, zs[c]});
            printf("PROBE: column (%d, z=%d):\n", xs[c], zs[c]);
            for (int y = 66; y <= 84; y++) {
                Worldgen_EvalY(&ctx, y);
                float id = worldgen.bounded && (y < worldgen.minY || y > worldgen.maxY)
                               ? 0 : Worldgen_Eval(&ctx, worldgen.material);
                printf("  y=%d material=%.1f\n", y, id);
            }
        }
    }
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
    {
        long mush = counts[64] + counts[65] + counts[66] + counts[73] +
                    counts[74] + counts[75];
        printf("PROBE: worldgen mushrooms 64-66/73-75 (must be 0): %ld\n", mush);
        if (mush != 0) { printf("PROBE: FAIL mushrooms in worldgen\n"); return 1; }
    }
    /* v63.9: every non-default id the worldgen places MUST have a block
     * definition - undefined ids slip past Worldgen_Freeze (the material
     * field is not validated) and render invisibly in the client */
    {
        long undef = 0;
        for (int i = 19; i < 256; i++) {
            if (counts[i] > 0 && !serverWorld.hasBlockDefinition[i]) {
                printf("PROBE: FAIL block %d used but not defined\n", i);
                undef++;
            }
        }
        if (undef) return 1;
        printf("PROBE: all used blocks defined\n");
    }
    printf("PROBE: starter-island biome-block cells (must be 0): %d\n", bad);
    printf("PROBE: OK\n");
    return 0;
}
