/*
 * worldprobe.c - v65 headless world audit.
 *
 * Generates REAL chunks through the real server pipeline (Lua mod ->
 * worldgen -> material rules -> ores -> structures -> features) into an
 * in-memory volume, then answers the questions that keep breaking the
 * cosmic worldgen:
 *
 *   1. what blocks does the world actually contain (histogram)?
 *   2. does every biome surface wear its own lawn, and how often?
 *   3. which plants grow on which biome ground (wrong-biome leaks)?
 *   4. how many sprite flora land in one chunk (the client draws at most
 *      CHUNK_FLORA_MAX of them - anything above that is invisible)?
 *   5. are plants stacked on plants, or floating over air?
 *   6. where do the log+leaves trees and the sprite trees stand?
 *   7. is any placed block id missing a definition (renders invisible)?
 *
 * Usage:  worldprobe [chunkRadius] [seed]
 *         (radius 4 = a 9x9 chunk column grid = 144x144 blocks)
 *
 * Not part of the shipped game. Build with tools_dev/probe_build.sh.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "scripting/luaengine.h"
#include "world/worldgen.h"
#include "world/worldgenerator.h"
#include "world.h"

extern World serverWorld;
void LuaBindings_Init(void);
void LuaBindings_Shutdown(void);

/* Must mirror CHUNK_FLORA_MAX in client/src/chunk/chunk.h - the client
 * collects sprite flora per chunk into a fixed array and silently drops
 * the rest, which is how whole lawns used to vanish. */
/* MUST mirror CHUNK_FLORA_MAX in client/src/chunk/chunk.h (tools_dev/
 * api_check.sh greps both so they can never drift apart again). */
#ifndef CLIENT_FLORA_CAP
#define CLIENT_FLORA_CAP 1024
#endif

#define MAX_Y 176

static const char *BlockName(int id) {
    static char buf[40];
    if (id > 0 && id < 256 && serverWorld.hasBlockDefinition[id])
        return serverWorld.blockDefinitions[id].name;
    switch (id) {
        case 0: return "air";
        case 1: return "stone";
        case 2: return "dirt";
        case 3: return "crystal_turf";
        case 4: return "wood";
        case 5: return "water";
        case 6: return "sand";
        case 7: return "iron_ore";
        case 8: return "coal_ore";
        case 9: return "gold_ore";
        case 10: return "log";
        case 11: return "leaves";
        case 12: return "rose";
        case 13: return "dandelion";
        case 14: return "glass";
        case 15: return "fire";
        case 16: return "lava";
        case 17: return "stone_slab";
        case 18: return "wood_slab";
    }
    snprintf(buf, sizeof(buf), "id%d", id);
    return buf;
}

/* sprite-model flora, exactly as the client classifies it */
static bool IsFlora(int id) {
    if (id <= 0 || id >= 256) return false;
    if (id == 12 || id == 13) return true;             /* client defaults */
    if (id == 15) return false;                        /* fire keeps quads */
    if (serverWorld.hasBlockDefinition[id])
        return serverWorld.blockDefinitions[id].modelType == BLOCK_MODEL_SPRITE;
    return false;
}

static bool IsLawn(int id) { return id == 39 || id == 41 || id == 59 || id == 60 || id == 32; }
static bool IsMushroom(int id) { return id == 73 || id == 74 || id == 75; }
static bool IsGround(int id) {
    return id == 1 || id == 2 || id == 3 || id == 19 || id == 56 || id == 57 || id == 58 ||
           id == 21 || id == 14 || id == 6 || id == 4 || id == 10;
}
/* which biome owns this ground block: 0 classic, 1 ember, 2 frost, -1 other */
static int BiomeOfGround(int id) {
    if (id == 3 || id == 2) return 0;
    if (id == 57 || id == 56) return 1;
    if (id == 58) return 2;
    return -1;
}
static const char *BiomeName(int b) {
    return b == 0 ? "classic" : b == 1 ? "ember" : b == 2 ? "frost" : "other";
}

typedef struct {
    int nx, ny, nz;
    int ox, oz;          /* world block coords of volume origin (0,0) */
    unsigned short *v;   /* [x * nz + z] * ny + y */
} Volume;

static inline unsigned short VolGet(const Volume *vol, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= vol->nx || y >= vol->ny || z >= vol->nz) return 0;
    return vol->v[(x * vol->nz + z) * vol->ny + y];
}

int main(int argc, char **argv) {
    int radius = argc > 1 ? atoi(argv[1]) : 4;
    int seed = argc > 2 ? atoi(argv[2]) : 20260913;
    /* v65.8: optional chunk-space centre, so the pocket universe region
     * (far from the origin) can be audited too: worldprobe R SEED CCX CCZ */
    int ccx = argc > 3 ? atoi(argv[3]) : 0;
    int ccz = argc > 4 ? atoi(argv[4]) : 0;
    if (radius < 1) radius = 1;
    if (radius > 12) radius = 12;

    /* exactly the singleplayer startup order: reset -> lua -> freeze */
    Worldgen_Reset(seed);
    Lua_Init();
    LuaBindings_Init();
    if (!Lua_Run()) {
        printf("PROBE: FAIL - mod did not load / worldgen did not freeze\n");
        return 1;
    }
    printf("PROBE: mod loaded; generator %s v%d; fields %d/%d; structures %d; ores %d; rules %d\n",
           worldgen.id, worldgen.version, worldgen.fieldCount, WG_MAX_FIELDS, worldgen.structureCount,
           worldgen.oreCount, worldgen.ruleCount);
    if (worldgen.material < 0) { printf("PROBE: FAIL - no material field\n"); return 1; }

    int ny = worldgen.maxY + 1;
    if (ny > MAX_Y) ny = MAX_Y;
    int chunkYs = (ny + CHUNK_SIZE_Y - 1) / CHUNK_SIZE_Y;
    int nxc = radius * 2 + 1;
    Volume vol;
    vol.nx = nxc * CHUNK_SIZE_X;
    vol.nz = nxc * CHUNK_SIZE_Z;
    vol.ny = ny;
    vol.ox = (ccx - radius) * CHUNK_SIZE_X;
    vol.oz = (ccz - radius) * CHUNK_SIZE_Z;
    vol.v = calloc((size_t)vol.nx * vol.nz * vol.ny, sizeof(unsigned short));
    if (!vol.v) { printf("PROBE: FAIL - out of memory\n"); return 1; }

    double t0 = GetTime();
    long chunkGens = 0;
    long floraPerChunkMax = 0, floraChunkSum = 0, floraChunks = 0, overCapChunks = 0;
    long droppedFlora = 0;
    for (int cz = 0; cz < nxc; cz++) {
        for (int cx = 0; cx < nxc; cx++) {
            for (int cy = 0; cy < chunkYs; cy++) {
                Chunk *chunk = ServerChunk_Create((Vector3){ (float)(cx - radius + ccx), (float)cy,
                                                             (float)(cz - radius + ccz) });
                Worldgen_Generate(chunk);
                chunkGens++;
                /* mirror the client's flora collection pass (chunkmeshgeneration.c) */
                long floraHere = 0;
                for (int i = 0; i < CHUNK_SIZE; i++) {
                    int id = chunk->data[i];
                    int lx = i % CHUNK_SIZE_X, ly = i / CHUNK_SIZE_XZ, lz = (i / CHUNK_SIZE_X) % CHUNK_SIZE_Z;
                    int wx = (int)chunk->blockPosition.x + lx, wy = (int)chunk->blockPosition.y + ly,
                        wz = (int)chunk->blockPosition.z + lz;
                    int vx = wx - vol.ox, vz = wz - vol.oz;
                    if (wy < ny && vx >= 0 && vz >= 0 && vx < vol.nx && vz < vol.nz)
                        vol.v[(vx * vol.nz + vz) * ny + wy] = (unsigned short)id;
                    if (IsFlora(id)) floraHere++;
                }
                if (floraHere > 0) {
                    floraChunks++;
                    floraChunkSum += floraHere;
                    if (floraHere > floraPerChunkMax) floraPerChunkMax = floraHere;
                    if (floraHere > CLIENT_FLORA_CAP) {
                        overCapChunks++;
                        droppedFlora += floraHere - CLIENT_FLORA_CAP;
                    }
                }
                ServerChunk_Destroy(chunk);
            }
        }
    }
    double t1 = GetTime();
    printf("PROBE: generated %ld chunks (%dx%d area, seed %d) in %.1fs\n",
           chunkGens, vol.nx, vol.nz, seed, t1 - t0);

    /* ---- 1. histogram ---- */
    long counts[256]; memset(counts, 0, sizeof(counts));
    for (long i = 0; i < (long)vol.nx * vol.nz * vol.ny; i++) counts[vol.v[i]]++;
    printf("PROBE: block histogram:\n");
    for (int i = 1; i < 256; i++)
        if (counts[i]) printf("   %3d %-20s %ld\n", i, BlockName(i), counts[i]);

    /* ---- 2/3. column audit: ground biome vs what grows on it ---- */
    long surfaceCols[3] = {0}, lawnCols[3] = {0}, bloomCols[3] = {0}, mushCols[3] = {0};
    long bareCols[3] = {0};
    long plantOnBiome[3][256]; memset(plantOnBiome, 0, sizeof(plantOnBiome));
    int mismatchEx[10][5], mismatchN = 0;
    long stacked = 0, floating = 0;
    int stackEx[6][4], floatEx[6][4]; int stackExN = 0, floatExN = 0;
    long treeStructures = 0, spriteTrees = 0;
    long treeBiome[3] = {0}, spriteTreeBiome[3] = {0}, treeOther = 0, spriteTreeOther = 0;

    for (int x = 0; x < vol.nx; x++) {
        for (int z = 0; z < vol.nz; z++) {
            /* walk the column, segment by segment (islands stack vertically) */
            int y = 0;
            while (y < vol.ny) {
                int id = VolGet(&vol, x, y, z);
                if (id == 0) { y++; continue; }
                /* found a solid cell: find the ground under any plant pile */
                if (id == 49 || id == 50) {
                    int base = VolGet(&vol, x, y - 1, z);
                    int tb = BiomeOfGround(base);
                    spriteTrees++;
                    if (tb >= 0) spriteTreeBiome[tb]++; else spriteTreeOther++;
                }
                if (IsFlora(id)) {
                    int below = VolGet(&vol, x, y - 1, z);
                    if (below == 0) {
                        floating++;
                        if (floatExN < 6) { floatEx[floatExN][0] = x; floatEx[floatExN][1] = y;
                                            floatEx[floatExN][2] = z; floatEx[floatExN][3] = id; floatExN++; }
                    } else if (IsFlora(below)) {
                        stacked++;
                        if (stackExN < 6) { stackEx[stackExN][0] = x; stackEx[stackExN][1] = y;
                                            stackEx[stackExN][2] = z; stackEx[stackExN][3] = id; stackExN++; }
                    }
                    y++;
                    continue;
                }
                /* a real ground cell: classify the surface it belongs to */
                int biome = BiomeOfGround(id);
                int above = VolGet(&vol, x, y + 1, z);
                bool leak = (biome == 0 && (above == 71 || above == 72 || above == 77 ||
                                            above == 75 || above == 60)) ||
                            (biome == 1 && (above == 31 || above == 33 || above == 39 ||
                                            above == 71 || above == 72 || above == 77)) ||
                            (biome == 2 && (above == 31 || above == 33 || above == 59 ||
                                            above == 67 || above == 68 || above == 76));
                if (biome >= 0 && leak && mismatchN < 10) {
                    mismatchEx[mismatchN][0] = x; mismatchEx[mismatchN][1] = y;
                    mismatchEx[mismatchN][2] = z; mismatchEx[mismatchN][3] = above;
                    mismatchEx[mismatchN][4] = id; mismatchN++;
                }
                if (biome >= 0 && (above == 0 || IsFlora(above))) {
                    surfaceCols[biome]++;
                    if (IsLawn(above)) { lawnCols[biome]++; plantOnBiome[biome][above]++; }
                    else if (IsMushroom(above)) { mushCols[biome]++; plantOnBiome[biome][above]++; }
                    else if (IsFlora(above)) { bloomCols[biome]++; plantOnBiome[biome][above]++; }
                    else bareCols[biome]++;
                }
                /* log+leaves tree (structure): count the trunk BASE only -
                 * upper trunk cells have trunk (not ground) below them */
                if (id == 10 && VolGet(&vol, x, y - 1, z) != 10) {
                    int base = VolGet(&vol, x, y - 1, z);
                    int tb = BiomeOfGround(base);
                    treeStructures++;
                    if (tb >= 0) treeBiome[tb]++; else treeOther++;
                }
                if (id == 49 || id == 50) {
                    int base = VolGet(&vol, x, y - 1, z);
                    int tb = BiomeOfGround(base);
                    spriteTrees++;
                    if (tb >= 0) spriteTreeBiome[tb]++; else spriteTreeOther++;
                }
                y++;
            }
        }
    }

    printf("PROBE: surface columns and what grows on them:\n");
    for (int b = 0; b < 3; b++) {
        if (!surfaceCols[b]) { printf("   %-8s (no surface found)\n", BiomeName(b)); continue; }
        printf("   %-8s surface=%ld  lawn=%ld (%.1f%%)  blooms=%ld (%.2f%%)  mushrooms=%ld (%.2f%%)  BARE=%ld (%.1f%%)\n",
               BiomeName(b), surfaceCols[b], lawnCols[b], 100.0 * lawnCols[b] / surfaceCols[b],
               bloomCols[b], 100.0 * bloomCols[b] / surfaceCols[b],
               mushCols[b], 100.0 * mushCols[b] / surfaceCols[b],
               bareCols[b], 100.0 * bareCols[b] / surfaceCols[b]);
        printf("            species:");
        for (int i = 1; i < 256; i++)
            if (plantOnBiome[b][i]) printf(" %s(%d)x%ld", BlockName(i), i, plantOnBiome[b][i]);
        printf("\n");
    }

    printf("PROBE: plant-over-ground samples (first %d flora columns):\n", mismatchN);
    for (int i = 0; i < mismatchN; i++)
        printf("   (%d,%d,%d) plant=%d(%s) ground=%d(%s)\n", mismatchEx[i][0], mismatchEx[i][1],
               mismatchEx[i][2], mismatchEx[i][3], BlockName(mismatchEx[i][3]),
               mismatchEx[i][4], BlockName(mismatchEx[i][4]));
    printf("PROBE: log+leaves tree trunks: %ld  (classic %ld / ember %ld / frost %ld / other-ground %ld)\n",
           treeStructures, treeBiome[0], treeBiome[1], treeBiome[2], treeOther);
    printf("PROBE: sprite trees (49/50):   %ld  (classic %ld / ember %ld / frost %ld / other-ground %ld)\n",
           spriteTrees, spriteTreeBiome[0], spriteTreeBiome[1], spriteTreeBiome[2], spriteTreeOther);

    printf("PROBE: flora per chunk: max=%ld avg=%.1f over %ld chunks; chunks above the client cap (%d)=%ld; flora dropped=%ld\n",
           floraPerChunkMax, floraChunks ? (double)floraChunkSum / floraChunks : 0.0,
           floraChunks, CLIENT_FLORA_CAP, overCapChunks, droppedFlora);

    printf("PROBE: stacked flora pairs=%ld, floating flora=%ld\n", stacked, floating);
    for (int i = 0; i < stackExN; i++)
        printf("   stack example: %s(%d) at (%d,%d,%d)\n", BlockName(stackEx[i][3]), stackEx[i][3],
               stackEx[i][0], stackEx[i][1], stackEx[i][2]);
    for (int i = 0; i < floatExN; i++)
        printf("   float example: %s(%d) at (%d,%d,%d)\n", BlockName(floatEx[i][3]), floatEx[i][3],
               floatEx[i][0], floatEx[i][1], floatEx[i][2]);

    /* ---- 6b. column dump (MIDLESS_DUMP_COLUMN="x,z" or "x,z x2,z2") ---- */
    {
        const char *env = getenv("MIDLESS_DUMP_COLUMN");
        if (env) {
            int dx, dz;
            const char *c = env;
            while (sscanf(c, " %d,%d", &dx, &dz) == 2) {
                printf("PROBE: column dump (%d,%d):\n", dx, dz);
                int vx = dx - vol.ox, vz = dz - vol.oz;
                if (vx >= 0 && vx < vol.nx && vz >= 0 && vz < vol.nz) {
                    for (int y = 0; y < vol.ny; y++) {
                        int id = VolGet(&vol, vx, y, vz);
                        if (id) printf("   y=%3d %d (%s)\n", y, id, BlockName(id));
                    }
                } else {
                    printf("   outside probe volume\n");
                }
                c = strchr(c, ',');
                if (!c) break;
                c = strchr(c + 1, ' ');
                if (!c) break;
            }
        }
    }

    /* ---- 7. undefined blocks ---- */
    long undefined = 0;
    for (int i = BLOCK_DEFAULT_LAST_ID + 1; i < 256; i++)
        if (counts[i] > 0 && !serverWorld.hasBlockDefinition[i]) {
            printf("PROBE: FAIL - block %d placed %ld times but never defined\n", i, counts[i]);
            undefined++;
        }

    /* ---- verdict ---- */
    int failures = 0;
    if (undefined) failures++;
    if (droppedFlora > 0) {
        printf("PROBE: FAIL - %ld flora per pass exceed the client's per-chunk billboard cap\n", droppedFlora);
        failures++;
    }
    for (int b = 0; b < 3; b++) {
        if (surfaceCols[b] && lawnCols[b] * 100 / surfaceCols[b] < 60) {
            printf("PROBE: FAIL - %s biome lawn covers under 60%% of its surface\n", BiomeName(b));
            failures++;
        }
        if (surfaceCols[b] && (bloomCols[b] + mushCols[b]) == 0) {
            printf("PROBE: FAIL - %s biome grows no blooms or mushrooms at all\n", BiomeName(b));
            failures++;
        }
    }
    if (stacked) { printf("PROBE: FAIL - plants stack on plants\n"); failures++; }
    if (floating) { printf("PROBE: FAIL - plants float over air\n"); failures++; }
    /* biome purity: v65 design - every biome grows ONLY its own species.
     * classic: roses/dandelions, the v54 six, tall reeds/bells, orchid,
     *          puff, stalk, both trees, glowcaps
     * ember:   ember tuft, smolderhead, cinder buds, ember lantern,
     *          cinder trumpet, lantern tree
     * frost:   frost tuft, frost burst, dewdrop, ringbloom, puffball,
     *          void tree */
    static const int allowed[3][24] = {
        { 12, 13, 28, 29, 30, 31, 32, 33, 37, 38, 39, 41, 47, 49, 50, 51, 52, 73, 0 },
        { 59, 67, 68, 76, 74, 50, 0 },
        { 60, 77, 71, 72, 75, 49, 0 },
    };
    for (int b = 0; b < 3; b++) {
        for (int i = 1; i < 256; i++) {
            if (!plantOnBiome[b][i]) continue;
            bool ok = false;
            for (int k = 0; allowed[b][k]; k++)
                if (allowed[b][k] == i) { ok = true; break; }
            if (!ok) {
                printf("PROBE: FAIL - %s(%d) grows on %s ground x%ld\n", BlockName(i), i,
                       BiomeName(b), plantOnBiome[b][i]);
                failures++;
            }
        }
    }
    if (treeBiome[1] || treeBiome[2]) {
        printf("PROBE: FAIL - green log/leaves trees stand on ember (%ld) or frost (%ld) ground\n",
               treeBiome[1], treeBiome[2]);
        failures++;
    }

    free(vol.v);
    LuaBindings_Shutdown();
    if (failures) { printf("PROBE: %d FAILURE GROUP(S)\n", failures); return 1; }
    printf("PROBE: OK\n");
    return 0;
}
