/**
 * test_moths.c - headless regression harness for the glowmoth/pollen system.
 *
 * It compiles the REAL client/src/mobs.c (copied into tests/build by
 * run_tests.sh so the stub headers in tests/stubs shadow the project ones)
 * against:
 *   - a stub world/player/hunter/etc., and
 *   - tests/rlmock.h, an exact model of raylib 4.5's rlgl batch renderer.
 *
 * Suites:
 *   1. detector  - replays the two historic bug patterns (7-vertex and
 *                  6-vertex "quads") and requires the model to flag both,
 *                  plus clean patterns that must stay silent;
 *   2. logic     - simulates play at 60 Hz and requires moths to spawn, fly,
 *                  lay a live falling pollen trail, with no NaNs;
 *   3. floor     - same over a solid island floor (bounced flight);
 *   4. render    - drives Mobs_Draw() every simulated frame through the
 *                  batch model (normal batch, tiny batch, and the shell-event
 *                  window) and requires ZERO shear events, exercised
 *                  overflow paths, and balanced/restored GL state.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>

#include "raylib.h"
#include "raymath.h"

/* batching model + its API surface (rlBegin, rlSetTexture, rlSetShader...) */
#include "stubs/rlgl.h"

/* stub interfaces (typedefs + prototypes) so signatures can be checked */
#include "stubs/player.h"
#include "stubs/world.h"
#include "stubs/i18n.h"
#include "stubs/hunter.h"
#include "stubs/soundfx.h"
#include "stubs/particle.h"
#include "stubs/chat.h"

/* ---------------- deterministic time & randomness ---------------- */
static double simTime = 0.0;
static double simStep = 1.0 / 60.0;
static unsigned int rngState = 0xC0FFEE42u;
int GetRandomValue(int min, int max) {
    rngState = rngState * 1664525u + 1013904223u;
    unsigned int span = (unsigned int)(max - min + 1);
    return min + (int)((rngState >> 8) % span);
}
double GetTime(void) { return simTime; }

/* ---------------- stub game state ---------------- */
Player player;
static int playerHeals = 0, playerDamages = 0, hotbarAdds = 0;
void Player_Heal(int amount) { player.hp += amount; if (player.hp > 10) player.hp = 10; playerHeals++; }
void Player_Damage(int amount, Vector3 push) { (void)amount; (void)push; playerDamages++; }
void Player_HotbarAutoAdd(int blockId) { (void)blockId; hotbarAdds++; }

static int airWorld = 1;   /* 1 = all air, 0 = island floor at y=70 */
static Vector3 cocoonCell;
static int cocoonPlaced = 0;
int World_GetBlock(Vector3 p) {
    if (cocoonPlaced &&
        (int)p.x == (int)cocoonCell.x && (int)p.y == (int)cocoonCell.y &&
        (int)p.z == (int)cocoonCell.z) return 25;   /* void cocoon */
    if (airWorld) return 0;
    return (p.y < 70.0f) ? 3 : 0;   /* solid floor */
}
static Vector3 lastSetBlock;
static int setBlockCalls = 0;
void World_SetBlock(Vector3 p, int id, bool send) {
    (void)send; lastSetBlock = p; setBlockCalls++;
    if (id == 0 && cocoonPlaced &&
        (int)p.x == (int)cocoonCell.x && (int)p.y == (int)cocoonCell.y &&
        (int)p.z == (int)cocoonCell.z) cocoonPlaced = 0;
}
void World_ExplodeAt(Vector3 cell) { (void)cell; }
static Texture2D fakeAtlas;
Texture2D World_GetTerrainTexture(void) { return fakeAtlas; }

float Hunter_GetSurgeLevel(void) { return 0.0f; }
static int shardDrops = 0, wireBursts = 0;
void Hunter_DropShards(Vector3 pos, int count) { (void)pos; shardDrops += count; }
void Hunter_WireBurst(Vector3 pos) { (void)pos; wireBursts++; }

static int impacts = 0;
void Particle_SpawnImpact(Vector3 position) { (void)position; impacts++; }

static int chatLines = 0;
void Chat_AddLine(const char *text) { (void)text; chatLines++; }
const char *Tr(const char *key) { return key; }

void SoundFx_PlayDig(void) {}
void SoundFx_PlayPlace(void) {}
void SoundFx_PlayJump(void) {}
void SoundFx_PlayTeleport(void) {}
void SoundFx_PlayHunterHit(void) {}
void SoundFx_PlayHunterDie(void) {}
void SoundFx_PlayWebAttach(void) {}
void SoundFx_PlayExplosion(void) {}
void SoundFx_PlayCocoonOpen(void) {}

/* block definitions: everything non-solid */
#include "block.h"
/* v63: grazers probe block colliders; the test build has no block.c */
Block blockDefinitions[256];
__attribute__((constructor)) static void test_block_defs(void) {
    static const int solids[] = {1, 2, 3, 19, 56, 57, 58};
    for (unsigned i = 0; i < sizeof(solids) / sizeof(solids[0]); i++)
        blockDefinitions[solids[i]].colliderType = BLOCK_COLLIDER_SOLID;
}

static Block stubBlock;
const Block *Block_GetDefinition(int id) { (void)id; return &stubBlock; }

/* raylib runtime stubs */
static Shader fakeShader;
Shader LoadShaderFromMemory(const char *vs, const char *fs) { (void)vs; (void)fs; return fakeShader; }
Texture2D LoadTextureFromImage(Image image) { (void)image; return fakeAtlas; }
void *MemAlloc(unsigned int size) { return malloc(size); }
void *MemRealloc(void *ptr, unsigned int size) { return realloc(ptr, size); }
void MemFree(void *ptr) { free(ptr); }
const char *TextFormat(const char *fmt, ...) {
    static char buf[512];
    va_list args; va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}
void BeginShaderMode(Shader s) { rlSetShader(s.id, NULL); }
void EndShaderMode(void) { rlSetShader(RLM_defaultShaderId, NULL); }
void UnloadTexture(Texture2D t) { (void)t; }
void SetTextureFilter(Texture2D t, int f) { (void)t; (void)f; }
float GetFrameTime(void) { return (float)simStep; }

/* ---------------- the real code under test ---------------- */
#include "mobs.c"

/* ---------------- tiny assertion framework ---------------- */
static int failures = 0, checks = 0;
#define REQUIRE(cond, ...) do { \
    checks++; \
    if (!(cond)) { failures++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } \
} while (0)

static void ResetWorld(int floorMode) {
    airWorld = floorMode;
    player.position = (Vector3){ 8.5f, 77.5f, 8.5f };
    player.flying = false;
    player.hp = 10;
    /* NOTE: simTime is NOT rewound - GetTime() is monotonic in the real
     * game, so a reset here models a world restart, not a time warp.
     * rngState is reseeded only to keep suites reproducible. */
    rngState = 0xC0FFEE42u;
    Mobs_Init();
}

/* count helpers - same TU as mobs.c so statics are visible */
static int CountActiveMoths(void) { return Mobs_MothCount(); }
static int CountLivePollen(void) {
    int n = 0;
    for (int i = 0; i < POLLEN_MAX; i++) if (pollen[i].life > 0.0f) n++;
    return n;
}
static int CountBadPollen(void) {
    int n = 0;
    for (int i = 0; i < POLLEN_MAX; i++) {
        if (pollen[i].life > 0.0f) {
            if (isnan(pollen[i].pos.x) || isnan(pollen[i].pos.y) || isnan(pollen[i].pos.z) ||
                fabsf(pollen[i].pos.x) > 100000.0f || fabsf(pollen[i].pos.y) > 100000.0f ||
                fabsf(pollen[i].pos.z) > 100000.0f) n++;
        }
    }
    return n;
}
static float PollenAvgVelY(void) {
    int n = 0; float sum = 0;
    for (int i = 0; i < POLLEN_MAX; i++) if (pollen[i].life > 0.0f) { sum += pollen[i].vel.y; n++; }
    return n ? sum / n : 0.0f;
}

/* ------------------------------------------------------------------ */
static void TestDetector(void) {
    printf("[1] shear detector validation\n");
    RLM_Reset();

    /* historic bug A: 7 vertices per pollen "quad" (the color-lines bug) */
    rlBegin(RL_QUADS); rlSetTexture(555);
    for (int i = 0; i < 5; i++) { for (int v = 0; v < 7; v++) rlVertex3f(0, 0, 0); }
    rlEnd(); rlDrawRenderBatchActive();
    REQUIRE(RLM_stats.shearEvents > 0, "7-vertex quads must be flagged (color-lines bug)");

    /* historic bug B: 6 vertices per quad (the half-quad bug) */
    RLM_Reset();
    rlBegin(RL_QUADS); rlSetTexture(555);
    for (int i = 0; i < 5; i++) { for (int v = 0; v < 6; v++) rlVertex3f(0, 0, 0); }
    rlEnd(); rlDrawRenderBatchActive();
    REQUIRE(RLM_stats.shearEvents > 0, "6-vertex quads must be flagged (half-quad bug)");

    /* clean: 4-vertex quads */
    RLM_Reset();
    rlBegin(RL_QUADS); rlSetTexture(555);
    for (int i = 0; i < 5; i++) { for (int v = 0; v < 4; v++) rlVertex3f(0, 0, 0); }
    rlEnd(); rlDrawRenderBatchActive();
    REQUIRE(RLM_stats.shearEvents == 0, "4-vertex quads must stay silent");

    /* clean: lines record followed by quads (rlgl pads lines records) */
    RLM_Reset();
    rlBegin(RL_LINES);
    for (int v = 0; v < 10; v++) rlVertex3f(0, 0, 0);   /* 5 lines -> pad 2 */
    rlEnd();
    rlBegin(RL_QUADS); rlSetTexture(555);
    for (int v = 0; v < 8; v++) rlVertex3f(0, 0, 0);
    rlEnd(); rlDrawRenderBatchActive();
    REQUIRE(RLM_stats.shearEvents == 0, "lines->quads transition must stay aligned");

    /* real-world v59.8 frame: a lone line record then quads */
    RLM_Reset();
    rlBegin(RL_LINES);
    for (int v = 0; v < 2; v++) rlVertex3f(0, 0, 0);
    rlEnd();
    rlBegin(RL_QUADS); rlSetTexture(555);
    for (int v = 0; v < 4; v++) rlVertex3f(0, 0, 0);
    rlEnd(); rlDrawRenderBatchActive();
    REQUIRE(RLM_stats.shearEvents == 0, "single-line->quads must stay aligned");
}

static void TestLogic(void) {
    printf("[2] moth logic (all-air world)\n");
    ResetWorld(1);

    int mothsAt10 = -1, pollenAt30 = -1, mothsAt45 = -1, pollenAt45 = -1;
    for (int f = 0; f < 45 * 60; f++) {
        simTime += simStep;
        Mobs_Update((float)simStep);
        if (f == 10 * 60 - 1) mothsAt10 = CountActiveMoths();
        if (f == 30 * 60 - 1) pollenAt30 = CountLivePollen();
        if (f == 45 * 60 - 1) { mothsAt45 = CountActiveMoths(); pollenAt45 = CountLivePollen(); }
    }
    REQUIRE(mothsAt10 >= 3, "at least 3 moths alive by t=10s (got %d)", mothsAt10);
    REQUIRE(mothsAt45 >= 3, "moths sustained by t=45s (got %d)", mothsAt45);
    REQUIRE(pollenAt30 >= 40, "live pollen trail by t=30s (got %d)", pollenAt30);
    REQUIRE(pollenAt45 >= 40, "live pollen trail by t=45s (got %d)", pollenAt45);
    REQUIRE(CountBadPollen() == 0, "no NaN/exploded pollen particles");

    /* falling: average vertical velocity of live pollen must be negative */
    float avgVy = PollenAvgVelY();
    REQUIRE(avgVy < -0.02f, "pollen must FALL on average (avg vel.y = %f)", avgVy);

    /* moths must actually MOVE: accumulate total distance over 3 s */
    Vector3 prev[MOTH_MAX];
    for (int i = 0; i < MOTH_MAX; i++) prev[i] = moths[i].pos;
    float totalMoved = 0.0f;
    for (int f = 0; f < 3 * 60; f++) {
        simTime += simStep;
        Mobs_Update((float)simStep);
        for (int i = 0; i < MOTH_MAX; i++) {
            if (moths[i].active) totalMoved += Vector3Distance(moths[i].pos, prev[i]);
            prev[i] = moths[i].pos;
        }
    }
    REQUIRE(totalMoved > 10.0f, "moths fly around (moved %.2f m in 3 s)", totalMoved);

    /* trail continuity: wrap-safe emission count over a 10 s window */
    int prevNext = pollenNext;
    long totalWrites = 0;
    for (int f = 0; f < 10 * 60; f++) {
        simTime += simStep;
        Mobs_Update((float)simStep);
        int d = pollenNext - prevNext;
        if (d != 0) {
            if (d < 0) d += POLLEN_MAX;
            totalWrites += d;
            prevNext = pollenNext;
        }
    }
    printf("    trail: %ld motes laid in the 10 s window\n", totalWrites);
    REQUIRE(totalWrites >= 300, "trail keeps flowing (10 s produced %ld motes, want >=300)", totalWrites);
}

static void TestLogicFloorWorld(void) {
    printf("[3] moth logic (island floor world)\n");
    ResetWorld(0);
    for (int f = 0; f < 60 * 60; f++) {
        simTime += simStep;
        Mobs_Update((float)simStep);
    }
    REQUIRE(Mobs_MothCount() >= 1, "moths exist above the islands (got %d)", Mobs_MothCount());
    REQUIRE(CountLivePollen() >= 20, "trail exists above the islands (got %d)", CountLivePollen());
    REQUIRE(CountBadPollen() == 0, "no NaN pollen in floor world");
}

static void TestRender(void) {
    printf("[4] render batch model (Mobs_Draw through rlgl 4.5 model)\n");
    ResetWorld(1);

    /* normal-size batch model */
    int framesWithDefaultTexPollen = 0;
    for (int f = 0; f < 60 * 25; f++) {
        simTime += simStep;
        Mobs_Update((float)simStep);
        RLM_lastError[0] = 0;
        RLM_stats.defaultTexRecords = 0;
        Mobs_Draw();
        REQUIRE(RLM_stats.shearEvents == 0, "air frame %d: %s", f, RLM_lastError);
        /* v61: the pollen must ride the PLAIN path on the default white
         * texture - that is the whole point of the rewrite */
        if (RLM_stats.defaultTexRecords > 0) framesWithDefaultTexPollen++;
    }
    REQUIRE(framesWithDefaultTexPollen >= 60 * 24,
            "pollen drawn on the default texture in %d of 1500 frames",
            framesWithDefaultTexPollen);

    /* shrunken buffers: force mid-primitive flushes + tiny draw-record lists */
    RLM_Reset();
    RLM_elementCount = 40;    /* 160 vertices per batch */
    RLM_maxDraws = 3;         /* flush on nearly every mode/texture switch */
    for (int f = 0; f < 60 * 20; f++) {
        simTime += simStep;
        Mobs_Update((float)simStep);
        RLM_lastError[0] = 0;
        Mobs_Draw();
        REQUIRE(RLM_stats.shearEvents == 0, "tiny-batch frame %d: %s", f, RLM_lastError);
    }
    REQUIRE(RLM_stats.overflowFlushes > 100,
            "tiny batch really exercised overflow paths (%d flushes)", RLM_stats.overflowFlushes);

    /* state balance: after a draw, depth mask & culling restored */
    RLM_Reset();
    simTime += simStep;
    Mobs_Update((float)simStep);
    Mobs_Draw();
    REQUIRE(RLM_depthMask == 1, "depth mask restored after Mobs_Draw");
    REQUIRE(RLM_backfaceCulling == 1, "backface culling restored after Mobs_Draw");
}

/* visibility profile - replicates Moth_PollenDraw's alpha for a mote */
static double PollenAlphaOf(const Pollen *p) {
    float k = Clamp(p->life, 0.0f, 1.0f);
    float ease = k * k * (3.0f - 2.0f * k);
    float twinkle = 0.92f + 0.08f * sinf((float)simTime * 3.4f + p->shift * 3.0f);
    return 235.0 * (0.60 + 0.40 * ease) * twinkle;
}

static void TestPollenVisibility(void) {
    printf("[5] pollen visibility profile (the \"pollen vanished\" bug)\n");
    ResetWorld(1);
    for (int f = 0; f < 30 * 60; f++) { simTime += simStep; Mobs_Update((float)simStep); }
    int n = 0; double sum = 0.0; double mn = 1e9, mx = -1e9;
    for (int i = 0; i < POLLEN_MAX; i++) {
        if (pollen[i].life <= 0.0f) continue;
        double a = PollenAlphaOf(&pollen[i]);
        sum += a; n++;
        if (a < mn) mn = a;
        if (a > mx) mx = a;
    }
    {   /* brief visibility telemetry */
        printf("    live motes: %d, alpha avg=%.1f min=%.1f max=%.1f (0-255)\n",
               n, sum / (n ? n : 1), mn, mx);
    }
    REQUIRE(n >= 60, "trail population exists (%d motes)", n);
    REQUIRE(sum / (n ? n : 1) >= 165.0,
            "average mote alpha %.1f must read on a dark sky", sum / (n ? n : 1));
    REQUIRE(mn >= 100.0, "even dying motes stay visible (min alpha %.1f)", mn);
    /* trail sinks gently, never free-falls nor rises away */
    float avgVy = PollenAvgVelY();
    REQUIRE(avgVy > -0.45f && avgVy < -0.05f, "dust hangs-and-falls (avg vel.y=%f)", avgVy);
    /* v61.1 invariant: the trail must never cover its own emitter */
    int covering = 0;
    for (int i = 0; i < POLLEN_MAX; i++) {
        if (pollen[i].life <= 0.0f) continue;
        for (int mI = 0; mI < MOTH_MAX; mI++) {
            if (!moths[mI].active) continue;
            if (Vector3Distance(pollen[i].pos, moths[mI].pos) < 0.14f) covering++;
        }
    }
    REQUIRE(covering == 0, "%d motes sit on top of a moth body", covering);
}

static void TestRenderShellEvent(void) {
    printf("[6] render through the shell event window (floor world, 175 s)\n");
    ResetWorld(0);
    for (int f = 0; f < 175 * 60; f++) {
        simTime += simStep;
        Mobs_Update((float)simStep);
        RLM_lastError[0] = 0;
        Mobs_Draw();
        REQUIRE(RLM_stats.shearEvents == 0, "shell frame %d: %s", f, RLM_lastError);
    }
    /* the violet shell event fires around t=75 s on a floor world; make
     * sure its texture-switching path was actually exercised */
    REQUIRE(shellAnnounced, "shell event activated during the run");
    REQUIRE(shellTexReady, "shell runtime textures were created");
}

static void TestSpiderRotation(void) {
    printf("[7] cocoon hatchery rotation (the \"empty cocoons\" bug)\n");
    ResetWorld(1);
    shardDrops = 0;

    /* six hatches in a row, at the game's real cadence (Cocoon_Scan
     * opens at most one cocoon per 0.5 s): every one must release a
     * spider, and ages must diverge so rotation always takes the oldest */
    for (int i = 0; i < 6; i++) {
        Vector3 pos = { 5.0f + i, 71.0f, 5.0f };
        bool ok = Mobs_SpawnSpider(pos);
        REQUIRE(ok, "hatch %d released a spider", i);
        for (int f = 0; f < 30; f++) { simTime += simStep; Mobs_Update((float)simStep); }
    }
    REQUIRE(Mobs_SpiderCount() == SPIDER_MAX,
            "population capped at SPIDER_MAX (got %d)", Mobs_SpiderCount());
    /* two oldest were rotated out: 2 collapses * 2 shard piles each */
    REQUIRE(shardDrops >= 4, "rotated-out spiders left shard loot (%d)", shardDrops);

    /* the NEWEST spiders must be the ones alive: last two spawn spots present */
    int found = 0;
    for (int i = 0; i < SPIDER_MAX; i++) {
        if (!spiders[i].active) continue;
        if ((int)spiders[i].pos.x == 9 || (int)spiders[i].pos.x == 10) found++;
    }
    REQUIRE(found == 2, "newest hatchlings alive at their cocoons (got %d)", found);

    /* and every live spider is younger than the rotated-out ones were */
    for (int i = 0; i < SPIDER_MAX; i++) {
        if (spiders[i].active)
            REQUIRE(spiders[i].age < 2.0f, "live spider %d is young (<2 s)", i);
    }
}

static void TestStarterSanctuary(void) {
    printf("[8] starter island sanctuary (no enemies at spawn)\n");
    ResetWorld(0);   /* floor world; player stands on the spawn pad */

    for (int f = 0; f < 60 * 45; f++) { simTime += simStep; Mobs_Update((float)simStep); }
    REQUIRE(Mobs_CrawlerCount() == 0, "no crawlers at the starter isle (got %d)", Mobs_CrawlerCount());
    REQUIRE(Mobs_WispCount() == 0, "no wisps at the starter isle (got %d)", Mobs_WispCount());

    /* a cocoon inside the sanctuary opens but releases nothing hostile */
    cocoonPlaced = 1;
    cocoonCell = (Vector3){ 10, 77, 10 };
    for (int f = 0; f < 60 * 3; f++) { simTime += simStep; Mobs_Update((float)simStep); }
    REQUIRE(cocoonPlaced == 0, "sanctuary cocoon still opened (no frozen props)");
    REQUIRE(Mobs_SpiderCount() == 0, "no spider emerges inside the sanctuary");

    /* walk far out: enemies live again, and a wild cocoon hatches */
    player.position = (Vector3){ 100.5f, 77.5f, 100.5f };
    cocoonPlaced = 1;
    cocoonCell = (Vector3){ 100, 77, 100 };
    for (int f = 0; f < 60 * 20; f++) { simTime += simStep; Mobs_Update((float)simStep); }
    REQUIRE(cocoonPlaced == 0, "wild cocoon hatched when approached");
    REQUIRE(Mobs_SpiderCount() >= 1, "hatchling emerged from the wild cocoon");
    REQUIRE(Mobs_CrawlerCount() >= 1, "crawlers roam far from the isle (got %d)", Mobs_CrawlerCount());
    cocoonPlaced = 0;
}

static void TestReset(void) {
    printf("[9] world reset hygiene\n");
    ResetWorld(1);
    for (int f = 0; f < 60 * 20; f++) { simTime += simStep; Mobs_Update((float)simStep); }
    REQUIRE(CountLivePollen() > 0, "trail alive before reset");
    Mobs_Init();
    REQUIRE(CountLivePollen() == 0, "Mobs_Init clears pollen");
    REQUIRE(CountActiveMoths() == 0, "Mobs_Init clears moths");
}

int main(void) {
    stubBlock.modelType = BLOCK_MODEL_GAS;
    stubBlock.colliderType = BLOCK_COLLIDER_SOLID;
    stubBlock.minBB = (Vector3){ 0, 0, 0 };
    stubBlock.maxBB = (Vector3){ 16, 16, 16 };
    fakeAtlas.id = 777;
    fakeShader.id = 333;

    TestDetector();
    TestLogic();
    TestLogicFloorWorld();
    TestRender();
    TestPollenVisibility();
    TestRenderShellEvent();
    TestSpiderRotation();
    TestStarterSanctuary();
    TestReset();

    printf("\n%s: %d checks, %d failures\n", failures ? "TESTS FAILED" : "TESTS OK", checks, failures);
    return failures ? 1 : 0;
}
