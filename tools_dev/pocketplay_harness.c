/* pocketplay harness (v65.8 fix cycle) - dev only.
 * Drives the REAL server stack headless with a synthetic local player:
 * places four warp cores, checks the fuse-into-gate morph, stands the
 * player on the gate and verifies the pocket-universe teleport (and the
 * way back through the pocket's centre gate). Not part of the game. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "raylib.h"
#include "scripting/luaengine.h"
#include "scripting/luabindings.h"
#include "world/world.h"
#include "player.h"

extern World serverWorld;
void RuntimePaths_Init(void) { /* harness runs from the sandbox cwd */ }
/* v65.32: probe stubs for the raylib file surface parkourmap.c touches */
const char *GetApplicationDirectory(void) { return ""; }
bool DirectoryExists(const char *path) { (void)path; return false; }

static int BlockAt(int x, int y, int z) {
    return ServerWorld_GetBlock((Vector3){ (float)x, (float)y, (float)z });
}

int main(void) {
    RuntimePaths_Init();
    Lua_Init();
    LuaBindings_Init();
    ServerWorld_Init();
    ServerNetwork_Init();
    if (!Lua_Run()) { printf("POCKETPLAY: mod load FAIL\n"); return 1; }
    LuaBindings_InvokeReady();

    printf("POCKETPLAY: definitions 78/79/80 = %d/%d/%d\n",
           serverWorld.hasBlockDefinition[78], serverWorld.hasBlockDefinition[79],
           serverWorld.hasBlockDefinition[80]);

    Player *player = ServerPlayer_Create(NULL, false);
    player->peer = player;
    player->name = strdup("Tester");
    ServerWorld_AddPlayer(player);

    /* generate the two arenas synchronously (the async loader needs the
     * real main loop; the harness only needs the chunks to exist) */
    Chunk *spawnChunk = ServerWorld_RequestChunk((Vector3){ 0, 4, 0 });
    Chunk *pocketChunk = ServerWorld_RequestChunk((Vector3){ 75, 9, -75 });
    /* the async loader normally attaches players; do it by hand so the
     * chunk manager keeps both arenas alive for the test */
    if (spawnChunk) ServerChunk_AddPlayer(spawnChunk, player);
    if (pocketChunk) ServerChunk_AddPlayer(pocketChunk, player);
    printf("POCKETPLAY: requestchunk spawn=%p pocket=%p block@8,77,8=%d turf@1204,154,-1196=%d\n",
           (void *)spawnChunk, (void *)pocketChunk,
           ServerWorld_GetBlock((Vector3){ 8, 77, 8 }),
           ServerWorld_GetBlock((Vector3){ 1204, 154, -1196 }));
    for (int i = 0; i < 20; i++) { ServerWorld_Update(); usleep(2000); }
    printf("POCKETPLAY: spawn chunk loaded = %d\n",
           ServerWorld_GetChunkAt((Vector3){ 0, 4, 0 }) != NULL);

    /* four warp cores in a 2x2 square at y=77 */
    Vector3 cells[4] = { { 8, 77, 8 }, { 9, 77, 8 }, { 8, 77, 9 }, { 9, 77, 9 } };
    for (int i = 0; i < 4; i++)
        ServerWorld_SetBlock(cells[i], 22, false, false, true);
    for (int i = 0; i < 10; i++) { ServerWorld_Update(); usleep(2000); }
    printf("POCKETPLAY: after morph cells = %d %d %d %d (expect 0 0 0 80 - gate where the 4th core landed)\n",
           BlockAt(8, 77, 8), BlockAt(9, 77, 8), BlockAt(8, 77, 9), BlockAt(9, 77, 9));

    /* stand on the gate */
    Entity *entity = &serverWorld.entities[player->entityId];
    ServerWorld_TeleportEntity(player->entityId, (Vector3){ 9.5f, 78.0f, 9.5f }, entity->rotation);
    for (int i = 0; i < 200; i++) { ServerWorld_Update(); usleep(2000); }
    entity = &serverWorld.entities[player->entityId];
    printf("POCKETPLAY: after standing on home gate pos = %.1f %.1f %.1f (expect ~1204.5 156 -1195.5)\n",
           entity->position.x, entity->position.y, entity->position.z);

    /* now stand on the pocket's centre gate (1200,154,-1200) */
    for (int i = 0; i < 20; i++) { ServerWorld_Update(); usleep(2000); }
    ServerWorld_TeleportEntity(player->entityId, (Vector3){ 1200.5f, 155.0f, -1199.5f }, entity->rotation);
    for (int i = 0; i < 400; i++) { ServerWorld_Update(); usleep(12000); }  /* > 2.5s cooldown */
    entity = &serverWorld.entities[player->entityId];
    printf("POCKETPLAY: after pocket gate pos = %.1f %.1f %.1f (expect ~10.5 78 10.5 beside the home gate)\n",
           entity->position.x, entity->position.y, entity->position.z);

    /* ---- v65.28/v65.29 act 3: a gate BUILT inside the meadow opens the
     * Foundry. Chunks only stay loaded around the player (far ones evict
     * and RequestChunk does not resurrect them headless), so the harness
     * walks the player there first, exactly like the real game does. ---- */
    entity = &serverWorld.entities[player->entityId];
    ServerWorld_TeleportEntity(player->entityId, (Vector3){ 1206.5f, 156.0f, -1196.5f }, entity->rotation);
    for (int i = 0; i < 500; i++) { ServerWorld_Update(); usleep(3000); }
    printf("POCKETPLAY: walked to the meadow, turf probe %d (expect 78)\n", BlockAt(1204, 154, -1196));

    Vector3 cores[4] = { { 1210, 155, -1198 }, { 1211, 155, -1198 },
                         { 1210, 155, -1197 }, { 1211, 155, -1197 } };
    for (int i = 0; i < 4; i++)
        ServerWorld_SetBlock(cores[i], 22, false, false, true);
    for (int i = 0; i < 40; i++) { ServerWorld_Update(); usleep(2000); }
    printf("POCKETPLAY: meadow-built fuse = %d %d %d %d (expect 0 0 0 80)\n",
           BlockAt(1210, 155, -1198), BlockAt(1211, 155, -1198),
           BlockAt(1210, 155, -1197), BlockAt(1211, 155, -1197));

    /* stand on the meadow-built gate */
    entity = &serverWorld.entities[player->entityId];
    ServerWorld_TeleportEntity(player->entityId, (Vector3){ 1211.5f, 156.0f, -1196.5f }, entity->rotation);
    for (int i = 0; i < 400; i++) { ServerWorld_Update(); usleep(12000); }  /* > 2.5s cooldown */
    entity = &serverWorld.entities[player->entityId];
    printf("POCKETPLAY: after meadow gate pos = %.1f %.1f %.1f (expect ~-1267.5 120 1204.5, the Foundry)\n",
           entity->position.x, entity->position.y, entity->position.z);

    /* ---- act 4: the Foundry spawns its chunks around the player ---- */
    for (int i = 0; i < 800; i++) { ServerWorld_Update(); usleep(4000); }
    printf("FOUNDRY: chunk alive = %d\n", ServerWorld_GetChunkAt((Vector3){ -75, 7, 75 }) != NULL);
    printf("FOUNDRY: centre gate(-1272,118,1200)=%d (expect 80)\n", BlockAt(-1272, 118, 1200));
    printf("FOUNDRY: court(-1266,118,1206)=%d (expect 81), below(-1266,114,1206)=%d (expect 82), air(-1266,122,1206)=%d (expect 0)\n",
           BlockAt(-1266, 118, 1206), BlockAt(-1266, 114, 1206), BlockAt(-1266, 122, 1206));
    printf("FOUNDRY: lamp path(-1276,118,1204)=%d (expect 83)\n", BlockAt(-1276, 118, 1204));
    /* chunk-level leak scan: real generated chunks around the spawn plaza */
    {
        int counts[256] = { 0 };
        for (int x = -1284; x < -1240; x++)
            for (int z = 1188; z < 1232; z++)
                for (int y = 119; y <= 145; y++) {
                    int id = BlockAt(x, y, z);
                    if (id > 0 && id != 80 && id != 81 && id != 82 && id != 83)
                        counts[id & 255]++;
                }
        printf("FOUNDRY CHUNK SCAN (leaks): ");
        int any = 0;
        for (int i = 0; i < 256; i++)
            if (counts[i]) { printf("id%d x%d  ", i, counts[i]); any = 1; }
        printf("%s\n", any ? "" : "clean");
    }
    /* the high course is validated cell-by-cell by tools_dev/foundryprobe.c
     * (frozen material field) - chunk reach here covers the spawn plaza */

    /* ---- act 5: the Foundry centre gate returns beside the meadow gate ---- */
    entity = &serverWorld.entities[player->entityId];
    ServerWorld_TeleportEntity(player->entityId, (Vector3){ -1271.5f, 119.0f, 1200.5f }, entity->rotation);
    for (int i = 0; i < 400; i++) { ServerWorld_Update(); usleep(12000); }
    entity = &serverWorld.entities[player->entityId];
    printf("POCKETPLAY: after foundry gate pos = %.1f %.1f %.1f (expect ~1212.5 156 -1195.5 beside the meadow gate)\n",
           entity->position.x, entity->position.y, entity->position.z);
    return 0;
}
