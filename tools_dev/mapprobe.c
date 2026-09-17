/* mapprobe (v65.32) - dev only. Drives the REAL server stack twice:
 * once with the built-in course, once with an in-memory user map
 * (ParkourMapSetSession) to prove: (a) the map overlay lands in the
 * generated chunks, (b) the Lua course switches to a plain slab,
 * (c) the map gate marker still produces the warp-gate block. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "raylib.h"
#include "parkourmap.h"
#include "scripting/luaengine.h"
#include "scripting/luabindings.h"
#include "world/world.h"
#include "player.h"

extern World serverWorld;
/* probe stubs for the raylib file surface parkourmap.c touches */
const char *GetApplicationDirectory(void) { return ""; }
bool DirectoryExists(const char *path) { (void)path; return false; }
void RuntimePaths_Init(void);
void RuntimePaths_Init(void) { /* probe runs from the sandbox cwd */ }

static int BlockAt(int x, int y, int z) {
    return ServerWorld_GetBlock((Vector3){ (float)x, (float)y, (float)z });
}

static int failures = 0;
static void Expect(const char *what, int got, int want) {
    printf("MAPPROBE: %-34s = %d (expect %d)%s\n", what, got, want,
           got == want ? "" : "  <-- FAIL");
    if (got != want) failures++;
}

int main(int argc, char **argv) {
    int useMap = (argc > 1 && strcmp(argv[1], "map") == 0);
    RuntimePaths_Init();

    if (useMap) {
        PMap m;
        memset(&m, 0, sizeof(m));
        snprintf(m.name, sizeof(m.name), "test");
        /* v65.37 anchor (-1392,118,1008), field 384x128x384:
         * crest landmark: local (222,9,192) -> world (-1170,127,1200) */
        m.boxes[m.boxCount++] = (PMapBox){ 222, 9, 192, 2, 2, 2, 83 };
        /* high platform away from the old course: (-1126,138,1244) */
        m.boxes[m.boxCount++] = (PMapBox){ 266, 20, 236, 3, 1, 3, 82 };
        m.gate[0] = 120; m.gate[1] = 0; m.gate[2] = 192; m.hasGate = true;
        m.start[0] = 126; m.start[1] = 0; m.start[2] = 196; m.hasStart = true;
        m.finish[0] = 266; m.finish[1] = 21; m.finish[2] = 236; m.hasFinish = true;
        ParkourMapSetSession(&m);
    }

    Lua_Init();
    LuaBindings_Init();
    ServerWorld_Init();
    ServerNetwork_Init();
    if (!Lua_Run()) { printf("MAPPROBE: mod load FAIL\n"); return 1; }
    LuaBindings_InvokeReady();

    Player *player = ServerPlayer_Create(NULL, false);
    player->peer = player;
    player->name = strdup("Tester");
    ServerWorld_AddPlayer(player);

    /* chunks only stay loaded around the player headless - walk to each
     * probe cluster exactly like pocketplay does, and read while there
     * (far chunks evict as soon as we walk on) */
    for (int i = 0; i < 40; i++) { ServerWorld_Update(); usleep(2000); }
    #define WALK(px, py, pz) do { \
        Entity *e = &serverWorld.entities[player->entityId]; \
        ServerWorld_TeleportEntity(player->entityId, (Vector3){ px, py, pz }, e->rotation); \
        for (int i = 0; i < 600; i++) { ServerWorld_Update(); usleep(3000); } \
    } while (0)

    WALK(-1270.5f, 120, 1200.5f);
    if (useMap) {
        Expect("map gate marker (-1272,118,1200)", BlockAt(-1272, 118, 1200), 80);
        Expect("gate pad (-1274,118,1200)", BlockAt(-1274, 118, 1200), 81);
        Expect("start pad (-1266,118,1204)", BlockAt(-1266, 118, 1204), 81);
        Expect("start pad edge (-1263,118,1207)", BlockAt(-1263, 118, 1207), 81);
        Expect("beyond pad is void (-1262,118,1204)", BlockAt(-1262, 118, 1204), 0);
    } else {
        Expect("default gate (-1272,118,1200)", BlockAt(-1272, 118, 1200), 80);
    }

    WALK(-1170.5f, 130, 1200.5f);
    if (useMap) {
        Expect("map crest box (-1170,127,1200)", BlockAt(-1170, 127, 1200), 83);
        Expect("no slab under crest (-1170,118,1200)", BlockAt(-1170, 118, 1200), 0);
    } else {
        Expect("default crest (-1170,127,1200)", BlockAt(-1170, 127, 1200), 81);
    }

    WALK(-1120.5f, 138, 1247.5f);
    if (useMap) {
        Expect("map platform (-1126,138,1244)", BlockAt(-1126, 138, 1244), 82);
        Expect("old course off (beacon spot)", BlockAt(-1114, 137, 1250), 0);
        Expect("old finish plateau gone", BlockAt(-1114, 135, 1250), 0);
    } else {
        Expect("default finish top (-1114,135,1250)", BlockAt(-1114, 135, 1250), 81);
        Expect("default beacon (-1114,137,1250)", BlockAt(-1114, 137, 1250), 83);
        Expect("air off course (-1126,138,1244)", BlockAt(-1126, 138, 1244), 0);
    }

    WALK(-1140.5f, 120, 1160.5f);
    if (useMap) Expect("zone is bare (-1140,118,1160)", BlockAt(-1140, 118, 1160), 0);
    else        Expect("air off course (-1140,118,1160)", BlockAt(-1140, 118, 1160), 0);
    printf("MAPPROBE: %s mode - %s\n", useMap ? "map" : "default",
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
