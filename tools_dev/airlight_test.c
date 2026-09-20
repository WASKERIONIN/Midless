/* airlight_test.c - v65.44 headless equivalence test for the pure-air
 * chunk fast path. White-box: includes the REAL chunk.c and
 * chunklightning.c (mesh_test.c pattern) and asserts that
 * Chunk_GenerateAir produces byte-identical sunlight/block-light fields
 * (and reconcile face flags) to the classic Chunk_Generate pipeline in
 * four scenarios:
 *   A. lone all-air chunk under open sky   (uniform memset path)
 *   B. all-air chunk under a solid chunk   (flood fallback, stays dark)
 *   C. air chunk BESIDE an overhang chunk  (reconcile must push the 15
 *                                           into the shadow - guards the
 *                                           incompleteSunlightFaces fix)
 *   D. emitter next door + dirty-list      (block light crosses over,
 *                                           World_MarkLightDirty lists it)
 *   E. airOnlyData mutation via Chunk_SetBlock
 * Dev only; not part of the game. */
#if !defined(PLATFORM_WEB)
    #define __clang__ true   /* stb_ds: same trick world.c uses for -std=c99 */
#endif
#define STB_DS_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "stb_ds.h"
#include "chunk.h"
#include "chunklightning.h"
#include "block.h"
#include "world.h"

/* ---- minimal raylib/network surface ---- */
int networkConnectedToServer = 1;   /* declared extern int by networkhandler.h */
void *MemAlloc(unsigned int size) { return calloc(1, size); }
void MemFree(void *p) { free(p); }
bool FileExists(const char *path) { (void)path; return false; }
unsigned char *LoadFileData(const char *path, unsigned int *len) { (void)path; *len = 0; return NULL; }
void UnloadFileData(unsigned char *data) { (void)data; }
bool SaveFileData(const char *path, void *data, unsigned int len) { (void)path; (void)data; (void)len; return false; }
static char tfBuf[4][512]; static int tfIdx;
const char *TextFormat(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    char *buf = tfBuf[tfIdx++ & 3];
    vsnprintf(buf, 512, fmt, args);
    va_end(args);
    return buf;
}
void ChunkMesh_Unload(ChunkMesh *mesh) { (void)mesh; }

/* ---- the REAL code under test (white-box includes) ---- */
#undef STB_DS_IMPLEMENTATION   /* the .c files include stb_ds.h again - decl only */
#include "../client/src/chunk/chunklightning.c"
#include "../client/src/chunk/chunk.c"

/* ---- minimal block table ---- */
Block blockDefinitions[256];
const Block *Block_GetDefinition(int id) { return &blockDefinitions[id]; }
bool Block_IsDefined(int id) { return id >= 0 && id < 256; }

static void SetupBlocks(void) {
    memset(blockDefinitions, 0, sizeof(blockDefinitions));
    Block *air = &blockDefinitions[0];
    strcpy(air->name, "air");
    air->renderType = BLOCK_RENDER_TRANSPARENT;
    air->colliderType = BLOCK_COLLIDER_NONE;
    air->lightPassFaces = 0x3F;
    Block *stone = &blockDefinitions[1];
    strcpy(stone->name, "stone");
    stone->renderType = BLOCK_RENDER_OPAQUE;
    stone->colliderType = BLOCK_COLLIDER_SOLID;
    stone->modelType = BLOCK_MODEL_SOLID;
    stone->fullCube = true;
    stone->fastOpaqueCube = true;
    stone->lightPassFaces = 0;
    Block *fire = &blockDefinitions[16];
    strcpy(fire->name, "fire");
    fire->renderType = BLOCK_RENDER_TRANSPARENT;
    fire->lightType = BLOCK_LIGHT_EMIT;
    fire->lightLevel = 15;
    fire->lightPassFaces = 0x3F;
}

/* ---- world surface mirrored from world.c (keep in sync!) ---- */
World world;

Chunk *World_GetChunkAt(Vector3 position) {
    long int p = Chunk_GetPackedPos(position);
    int index = hmgeti(world.chunks, p);
    if (index >= 0) return world.chunks[index].value;
    return NULL;
}

void World_MarkLightDirty(Chunk *chunk) {
    if (chunk->isLightDirty) return;
    chunk->isLightDirty = true;
    if (chunk->isBuilt) arrput(world.lightDirtyChunks, chunk);
}

static void ResetWorld(void) {
    for (int i = hmlen(world.chunks) - 1; i >= 0; i--) Chunk_Destroy(world.chunks[i].value);
    hmfree(world.chunks);
    world.chunks = NULL;
    arrfree(world.lightDirtyChunks);
    world.lightDirtyChunks = NULL;
}

/* same order as World_AddChunk: create (wires neighbours) then register */
static Chunk *AddTestChunk(int x, int y, int z) {
    Vector3 pos = { (float)x, (float)y, (float)z };
    Chunk *chunk = Chunk_Create(pos);
    if (!chunk) return NULL;
    memset(chunk->data, 0, sizeof(chunk->data));
    hmput(world.chunks, Chunk_GetPackedPos(pos), chunk);
    return chunk;
}

static void FillStone(Chunk *c, int x0, int y0, int z0, int x1, int y1, int z1) {
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                c->data[(y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x] = 1;
}

static void SetSky(Chunk *c, bool open) {
    memset(c->skyMask, open ? 0xFF : 0x00, CHUNK_SKY_MASK_SIZE);
}

static int failures;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok:   %s\n", msg); } \
} while (0)

static bool SameLight(const Chunk *a, const Chunk *b) {
    return memcmp(a->sunlightData, b->sunlightData, sizeof(a->sunlightData)) == 0 &&
           memcmp(a->lightData, b->lightData, sizeof(a->lightData)) == 0 &&
           a->incompleteSunlightFaces == b->incompleteSunlightFaces &&
           a->incompleteLightFaces == b->incompleteLightFaces;
}

/* world A: air chunk via the FAST path; world B: via the CLASSIC path.
 * build: 0 = lone air, 1 = air under stone, 2 = air beside overhang,
 * 3 = air beside emitter */
static void RunScenarioPair(int scenario, Chunk **outAirFast, Chunk **outAirFull,
                            Chunk **outSolidFast, Chunk **outSolidFull) {
    static Chunk airFastCopy, solidFastCopy, airFullCopy, solidFullCopy;
    for (int pass = 0; pass < 2; pass++) {
        bool fast = (pass == 0);
        Chunk *air, *solid = NULL;
        if (scenario == 0) {
            air = AddTestChunk(0, 0, 0);
            SetSky(air, true);
            if (fast) Chunk_GenerateAir(air); else Chunk_Generate(air);
        } else if (scenario == 1) {
            solid = AddTestChunk(0, 1, 0);
            SetSky(solid, true);
            FillStone(solid, 0, 0, 0, 15, 15, 15);
            Chunk_Generate(solid);
            air = AddTestChunk(0, 0, 0);
            SetSky(air, false);          /* covered from above */
            if (fast) Chunk_GenerateAir(air); else Chunk_Generate(air);
        } else if (scenario == 2) {
            /* air A at z-1 beside solid S; S carries an overhang ON THE
             * A SIDE (stone y>=8, z<=7) so its shadow cells touch the
             * boundary - only reconcile can bring A's 15 into them */
            air = AddTestChunk(0, 0, -1);
            SetSky(air, true);
            if (fast) Chunk_GenerateAir(air); else Chunk_Generate(air);
            solid = AddTestChunk(0, 0, 0);
            SetSky(solid, true);
            FillStone(solid, 0, 8, 0, 15, 15, 7);
            Chunk_Generate(solid);
        } else {
            /* air beside an air chunk holding a fire block at the border */
            air = AddTestChunk(0, 0, -1);
            SetSky(air, true);
            air->isBuilt = true;         /* exercise the dirty list */
            if (fast) Chunk_GenerateAir(air); else Chunk_Generate(air);
            solid = AddTestChunk(0, 0, 0);
            SetSky(solid, true);
            solid->data[(4 * CHUNK_SIZE_Z + 0) * CHUNK_SIZE_X + 8] = 16;  /* fire at z=0 */
            Chunk_Generate(solid);
        }
        if (fast) {
            airFastCopy = *air; *outAirFast = &airFastCopy;
            if (solid) { solidFastCopy = *solid; *outSolidFast = &solidFastCopy; }
            else *outSolidFast = NULL;
            ResetWorld();
        } else {
            airFullCopy = *air; *outAirFull = &airFullCopy;
            if (solid) { solidFullCopy = *solid; *outSolidFull = &solidFullCopy; }
            else *outSolidFull = NULL;
            ResetWorld();
        }
    }
}

int main(void) {
    SetupBlocks();

    /* ---- scenario A: lone all-air chunk, open sky ---- */
    {
        Chunk *af, *aF, *sf, *sF;
        RunScenarioPair(0, &af, &aF, &sf, &sF);
        CHECK(SameLight(af, aF), "A: fast path == classic path (sunlight, light, flags)");
        bool all15 = true;
        for (int i = 0; i < CHUNK_SIZE; i++) if (af->sunlightData[i] != 15) all15 = false;
        CHECK(all15, "A: open-sky air is sunlit 15 everywhere");
        CHECK(af->incompleteSunlightFaces == 0x3F, "A: all faces flagged incomplete for later reconcile");
        ResetWorld();
    }

    /* ---- scenario B: air under a solid chunk ---- */
    {
        Chunk *af, *aF, *sf, *sF;
        RunScenarioPair(1, &af, &aF, &sf, &sF);
        CHECK(SameLight(af, aF), "B: covered air - fast == classic");
        bool all0 = true;
        for (int i = 0; i < CHUNK_SIZE; i++) if (af->sunlightData[i] != 0) all0 = false;
        CHECK(all0, "B: fully covered underside stays dark (no fake 15)");
        ResetWorld();
    }

    /* ---- scenario C: overhang shadow fed through reconcile ---- */
    {
        Chunk *af, *aF, *sf, *sF;
        RunScenarioPair(2, &af, &aF, &sf, &sF);
        CHECK(SameLight(af, aF), "C: air side - fast == classic");
        CHECK(SameLight(sf, sF), "C: overhang chunk lit identically in both worlds");
        /* sensitivity: the shadow cell touching the border must be bright */
        int edgeIdx = (2 * CHUNK_SIZE_Z + 0) * CHUNK_SIZE_X + 8;   /* y=2, z=0 */
        CHECK(sf->sunlightData[edgeIdx] >= 13,
              "C: shadow cell at the air boundary received lateral sunlight (>=13)");
        ResetWorld();
    }

    /* ---- scenario D: emitter next door + dirty list ---- */
    {
        Chunk *af, *aF, *sf, *sF;
        RunScenarioPair(3, &af, &aF, &sf, &sF);
        CHECK(SameLight(af, aF), "D: air side block light - fast == classic");
        int borderIdx = (4 * CHUNK_SIZE_Z + 15) * CHUNK_SIZE_X + 8; /* A-local cell beside the fire */
        CHECK(af->lightData[borderIdx] > 0, "D: fire light crossed into the air chunk");
        ResetWorld();
    }

    /* ---- dirty-list semantics on the REAL mirrored function ---- */
    {
        Chunk *c = AddTestChunk(5, 5, 5);
        c->isBuilt = false;
        Chunk_SetLightLevel(c, 0, 9, false);
        CHECK(arrlen(world.lightDirtyChunks) == 0, "dirty list ignores unbuilt chunks");
        c->isBuilt = true;
        c->isLightDirty = false;
        Chunk_SetLightLevel(c, 1, 9, false);
        Chunk_SetLightLevel(c, 2, 9, false);   /* second mark must not duplicate */
        CHECK(arrlen(world.lightDirtyChunks) == 1 && world.lightDirtyChunks[0] == c,
              "dirty list holds a built dirtied chunk exactly once");
        ResetWorld();
    }

    /* ---- E: airOnlyData mutation ---- */
    {
        Chunk *c = AddTestChunk(9, 9, 9);
        c->airOnlyData = true;
        c->isLightGenerated = true;
        Chunk_SetBlock(c, (Vector3){ 3, 3, 3 }, 1);
        CHECK(c->airOnlyData == false, "E: placing a block ends the air fast path");
        CHECK(c->data[(3 * CHUNK_SIZE_Z + 3) * CHUNK_SIZE_X + 3] == 1, "E: block stored");
        Chunk_SetBlock(c, (Vector3){ 3, 3, 3 }, 0);
        CHECK(c->airOnlyData == false, "E: removing it again does not resurrect the flag");
        ResetWorld();
    }

    printf(failures ? "AIRLIGHT: %d FAILURES\n" : "AIRLIGHT: ALL PASS (%d failures)\n", failures);
    return failures ? 1 : 0;
}
