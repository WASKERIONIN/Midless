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
    return 0;
}
