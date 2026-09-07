#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "raylib.h"
#include "minilua.h"
#include "world.h"
#include "worldgen.h"
#include "worldgenerator.h"
#include "chunk/chunk.h"

extern lua_State *L;
void LuaWorldgen_Init(void);

static int failures;
static void Check(bool ok, const char *what) {
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}
static void Info(const char *what) { printf("  [INFO] %s\n", what); }
static double Now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
static void MakeChunk(Chunk *c, int cx, int cy, int cz) {
    memset(c, 0, sizeof(*c));
    c->position = (Vector3){cx, cy, cz};
    c->blockPosition = (Vector3){cx * CHUNK_SIZE_X, cy * CHUNK_SIZE_Y, cz * CHUNK_SIZE_Z};
}
static int Idx(int x, int y, int z) { return (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x; }

/* E1 -- does the per-column evaluation cache used by Worldgen_Generate agree
 * with evaluating the same field through a fresh context?  The loop below is a
 * literal copy of the production loop (fresh Worldgen_EvalInit per column,
 * position == origin == (x, chunkBaseY, z), then Worldgen_EvalY per y). */
static void TestFieldCacheConsistency(int cx, int cy, int cz) {
    int mismatch = 0, compared = 0;
    float worstDelta = 0;
    Vector3 worstAt = {0}, worstValues = {0};
    for (int z = cz * 16; z < cz * 16 + 16; z++)
        for (int x = cx * 16; x < cx * 16 + 16; x++) {
            WGEval context;
            Worldgen_EvalInit(&context, (Vector3){x, cy * 16, z}, (Vector3){x, cy * 16, z});
            for (int y = cy * 16; y < cy * 16 + 16; y++) {
                Worldgen_EvalY(&context, y);
                float cached = Worldgen_Eval(&context, worldgen.material);
                float fresh = Worldgen_Field(worldgen.material, x, y, z);
                compared++;
                float delta = fabsf(cached - fresh);
                if (delta > worstDelta) {
                    worstDelta = delta;
                    worstAt = (Vector3){x, y, z};
                    worstValues = (Vector3){cached, fresh, 0};
                }
                if (delta > 0) mismatch++;
            }
        }
    char msg[256];
    snprintf(msg, sizeof(msg),
             "chunk(%d,%d,%d): column cache == fresh evaluation (%d samples, %d mismatches; worst "
             "%g vs %g at (%g,%g,%g))",
             cx, cy, cz, compared, mismatch, worstValues.x, worstValues.y, worstAt.x, worstAt.y,
             worstAt.z);
    Check(mismatch == 0, msg);
}

/* E2 -- generating the same set of chunks in a different order must not change
 * any block.  Catches cross-chunk state (feature origin cache, halo, ordering). */
static void TestOrderIndependence(int radius) {
    int count = 0;
    static Chunk forward[512], backward[512];
    int positions[512][3];
    for (int y = 2; y <= 5; y++)
        for (int z = -radius; z <= radius; z++)
            for (int x = -radius; x <= radius; x++) {
                if (count >= 512) break;
                positions[count][0] = x;
                positions[count][1] = y;
                positions[count][2] = z;
                count++;
            }
    for (int i = 0; i < count; i++) {
        MakeChunk(&forward[i], positions[i][0], positions[i][1], positions[i][2]);
        Worldgen_Generate(&forward[i]);
    }
    for (int i = count - 1; i >= 0; i--) {
        MakeChunk(&backward[i], positions[i][0], positions[i][1], positions[i][2]);
        Worldgen_Generate(&backward[i]);
    }
    int differing = 0, firstBad = -1;
    for (int i = 0; i < count; i++)
        if (memcmp(forward[i].data, backward[i].data, sizeof(forward[i].data)) != 0) {
            if (firstBad < 0) firstBad = i;
            differing++;
        }
    char msg[192];
    snprintf(msg, sizeof(msg),
             "generation order independent (%d chunks; %d differ, first at (%d,%d,%d))", count,
             differing, firstBad < 0 ? 0 : positions[firstBad][0],
             firstBad < 0 ? 0 : positions[firstBad][1], firstBad < 0 ? 0 : positions[firstBad][2]);
    Check(differing == 0, msg);
}

/* E3 -- sky mask vs brute-force vertical scan of the skylight field. */
static void TestSkyMask(int cx, int cy, int cz) {
    if (worldgen.skyField < 0) {
        Info("sky mask: no skylight field configured, skipped");
        return;
    }
    static Chunk c;
    MakeChunk(&c, cx, cy, cz);
    Worldgen_Generate(&c);
    Worldgen_SkyMask(&c);
    int wrong = 0;
    int firstX = 0, firstZ = 0;
    for (int z = 0; z < CHUNK_SIZE_Z; z++)
        for (int x = 0; x < CHUNK_SIZE_X; x++) {
            bool blocked = false;
            for (int y = c.blockPosition.y + CHUNK_SIZE_Y; y <= worldgen.maxY && !blocked; y++)
                if (Worldgen_Field(worldgen.skyField, c.blockPosition.x + x, y,
                                   c.blockPosition.z + z) > 0)
                    blocked = true;
            int column = z * CHUNK_SIZE_X + x;
            bool maskSaysVisible = c.skyMask[column >> 3] & (1u << (column & 7));
            if (blocked == maskSaysVisible) {
                if (!wrong) {
                    firstX = c.blockPosition.x + x;
                    firstZ = c.blockPosition.z + z;
                }
                wrong++;
            }
        }
    char msg[192];
    snprintf(msg, sizeof(msg),
             "skyMask matches brute-force sky scan at chunk(%d,%d,%d) (%d/%d columns wrong, first "
             "at x=%d z=%d)",
             cx, cy, cz, wrong, CHUNK_SIZE_XZ, firstX, firstZ);
    Check(wrong == 0, msg);
}

/* E4 -- RLE round trip on real generated data. */
static void TestCompression(const Chunk *c, const char *label) {
    int length = 0;
    unsigned short *compressed = ChunkData_CreateCompressed(c->data, &length);
    unsigned short restored[CHUNK_SIZE];
    memset(restored, 0xff, sizeof(restored));
    bool ok = compressed && ChunkData_Decompress(restored, compressed, length) &&
              memcmp(restored, c->data, sizeof(c->data)) == 0;
    char msg[192];
    snprintf(msg, sizeof(msg), "%s: RLE round trip (8192 bytes -> %d bytes)", label, length * 2);
    Check(ok, msg);
    free(compressed);
}

/* E5 -- the heightmap path must lay out top/filler/stone purely as a function
 * of depth below the surface column, regardless of chunk boundaries. */
static bool overrideId[256];
static void CollectOverrideIds(void) {
    memset(overrideId, 0, sizeof(overrideId));
    for (int i = 0; i < worldgen.ruleCount; i++) overrideId[worldgen.rules[i].block] = true;
    for (int i = 0; i < worldgen.oreCount; i++) overrideId[worldgen.ores[i].block] = true;
    for (int i = 0; i < worldgen.structureCount; i++) {
        overrideId[worldgen.structures[i].foundation] = true;
        for (int j = 0; j < worldgen.structures[i].count; j++)
            overrideId[worldgen.structures[i].blocks[j].id] = true;
    }
    for (int i = 0; i < worldgen.featureCount; i++)
        for (int j = 0; j < worldgen.features[i].count; j++)
            overrideId[worldgen.features[i].commands[j].block] = true;
}

static void TestSurfaceLayers(void) {
    CollectOverrideIds();
    if (worldgen.biomeCount > 1) {
        Info("surface layering: multi-biome definition (filler depth varies per column), skipped");
        return;
    }
    if (worldgen.material >= 0) {
        Info("surface layers: material-field worldgen, halo path not used, skipped");
        return;
    }
    int checked = 0, wrong = 0;
    int firstX = 0, firstY = 0, firstZ = 0, firstGot = 0, firstWant = 0;
    for (int cz = -2; cz <= 2; cz++)
        for (int cx = -2; cx <= 2; cx++) {
            static Chunk stack[16];
            for (int s = 0; s < 16; s++) {
                MakeChunk(&stack[s], cx, s - 4, cz);
                Worldgen_Generate(&stack[s]);
            }
            for (int z = 0; z < 16; z++)
                for (int x = 0; x < 16; x++) {
                    int wx = cx * 16 + x, wz = cz * 16 + z;
                    /* Surface = highest natural terrain block; ignore air, water
                     * and anything a rule/ore/structure/feature may have placed
                     * (e.g. tree leaves floating above the ground). */
                    int surface = -100000;
                    for (int s = 15; s >= 0 && surface == -100000; s--)
                        for (int y = 15; y >= 0; y--) {
                            int id = stack[s].data[Idx(x, y, z)];
                            if (id != 0 && id != 5 && !overrideId[id]) {
                                surface = (s - 4) * 16 + y;
                                break;
                            }
                        }
                    if (surface == -100000) continue;
                    const WGBiome *b = &worldgen.biomes[0];
                    for (int s = 0; s < 16; s++)
                        for (int y = 0; y < 16; y++) {
                            int wy = (s - 4) * 16 + y;
                            int got = stack[s].data[Idx(x, y, z)];
                            if (wy > surface) continue;
                            int depth = surface - wy;
                            int want;
                            if (depth == 0)
                                want = surface <= worldgen.seaLevel ? b->underwater : b->top;
                            else if (depth <= b->depth)
                                want = b->filler;
                            else
                                want = b->stone;
                            if (got == 5) continue;      /* water column above terrain */
                            if (overrideId[got]) continue; /* rewritten by rule/ore/structure */
                            checked++;
                            if (got != want && !wrong) {
                                wrong = 1;
                                firstX = wx;
                                firstY = wy;
                                firstZ = wz;
                                firstGot = got;
                                firstWant = want;
                            }
                        }
                }
        }
    char msg[224];
    snprintf(msg, sizeof(msg),
             "heightmap surface layering is boundary independent (%d samples, first violation at "
             "(%d,%d,%d): got %d want %d)",
             checked, firstX, firstY, firstZ, firstGot, firstWant);
    Check(!wrong, msg);
}

/* E6 -- a rule with a vertical offset cannot reach across a chunk boundary, so
 * count the grass columns whose block below was left untouched. */
static void TestRuleSeam(void) {
    if (!worldgen.ruleCount) {
        Info("rule seam: no rules defined, skipped");
        return;
    }
    int ruleOffset = 0;
    for (int i = 0; i < worldgen.ruleCount; i++)
        if (worldgen.rules[i].match == 3 && worldgen.rules[i].block == 9)
            ruleOffset = worldgen.rules[i].dy;
    if (!ruleOffset) {
        Info("rule seam: no grass->9 rule in this definition, skipped");
        return;
    }
    static Chunk stack[8];
    for (int s = 0; s < 8; s++) {
        MakeChunk(&stack[s], 0, s, 0);
        Worldgen_Generate(&stack[s]);
    }
    int grass = 0, missed = 0, missedAtLocalY = -1;
    for (int s = 1; s < 8; s++)
        for (int y = 0; y < 16; y++)
            for (int z = 0; z < 16; z++)
                for (int x = 0; x < 16; x++) {
                    if (stack[s].data[Idx(x, y, z)] != 3) continue;
                    grass++;
                    int below = stack[s].data[Idx(x, y - 1, z)];
                    if (below != 9) {
                        missed++;
                        missedAtLocalY = y;
                    }
                }
    char msg[224];
    snprintf(msg, sizeof(msg),
             "rule offset_y=%d reached every grass column (%d grass columns, %d missed, last miss "
             "at local y=%d inside its chunk)",
             ruleOffset, grass, missed, missedAtLocalY);
    Check(missed == 0, msg);
}

static bool LoadMod(const char *path) {
    lua_newtable(L);
    lua_setglobal(L, "midless");
    LuaWorldgen_Init();
    if (luaL_dofile(L, path) != 0) {
        printf("lua error in %s: %s\n", path, lua_tostring(L, -1));
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    const char *mod = argc > 1 ? argv[1] : NULL;
    int seed = argc > 2 ? atoi(argv[2]) : 1337;
    mkdir("world", 0755);

    ServerWorldGenerator_Init(seed);
    printf("\n== worldgen self-check: %s (seed %d) ==\n", mod ? mod : "(builtin, no mod)", seed);
    if (mod) {
        L = luaL_newstate();
        luaL_openlibs(L);
        if (!LoadMod(mod)) return 1;
    }
    if (!Worldgen_Freeze()) {
        printf("Worldgen_Freeze() FAILED\n");
        return 1;
    }
    printf("definition: %s v%d bounded=%d material=%d sky=%d ceiling=%d minY=%d maxY=%d sea=%d "
           "fields=%d biomes=%d ores=%d structures=%d rules=%d features=%d\n",
           worldgen.id, worldgen.version, worldgen.bounded, worldgen.material, worldgen.skyField,
           worldgen.ceiling, worldgen.minY, worldgen.maxY, worldgen.seaLevel, worldgen.fieldCount,
           worldgen.biomeCount, worldgen.oreCount, worldgen.structureCount, worldgen.ruleCount,
           worldgen.featureCount);

    if (worldgen.material >= 0) {
        printf("-- field cache consistency --\n");
        TestFieldCacheConsistency(0, 4, 0);
        TestFieldCacheConsistency(3, 4, 2);
        TestFieldCacheConsistency(-4, 1, -4);
    }
    printf("-- sky mask --\n");
    TestSkyMask(0, 4, 0);
    printf("-- surface layering --\n");
    TestSurfaceLayers();
    printf("-- rule reach --\n");
    TestRuleSeam();
    printf("-- compression --\n");
    {
        static Chunk c;
        MakeChunk(&c, 0, 4, 0);
        Worldgen_Generate(&c);
        TestCompression(&c, "generated chunk");
    }
    printf("-- order independence --\n");
    TestOrderIndependence(1);
    printf("-- throughput --\n");
    {
        double t0 = Now();
        int n = 0;
        for (int x = -2; x <= 2; x++)
            for (int z = -2; z <= 2; z++)
                for (int y = 2; y <= 5; y++) {
                    static Chunk c;
                    MakeChunk(&c, x, y, z);
                    Worldgen_Generate(&c);
                    Worldgen_SkyMask(&c);
                    n++;
                }
        double dt = Now() - t0;
        printf("  [INFO] %d chunks (generate + skymask) in %.3fs -> %.1f ms/chunk\n", n, dt,
               dt * 1000 / n);
    }
    printf("-- sky-mask fallback cost (chunks above the terrain, no skylight field) --\n");
    if (worldgen.skyField < 0) {
        double t0 = Now();
        int n = 0;
        for (int x = -1; x <= 1; x++)
            for (int z = -1; z <= 1; z++)
                for (int y = 7; y <= 9; y++) {
                    static Chunk c;
                    MakeChunk(&c, x, y, z);
                    Worldgen_Generate(&c);
                    Worldgen_SkyMask(&c);
                    n++;
                }
        double dt = Now() - t0;
        printf("  [INFO] %d high chunks in %.3fs -> %.1f ms/chunk (maxY=%d)\n", n, dt,
               dt * 1000 / n, worldgen.maxY);
    } else {
        Info("skylight field present, chunk-above regeneration path not used");
    }
    printf("%s (%d failing checks)\n", failures ? "PROBLEMS FOUND" : "ALL CHECKS PASSED", failures);
    return failures != 0;
}
