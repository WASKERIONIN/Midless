#include "parkourmap.h"   /* v65.34: editor TEST warp (player.h comes below) */
#include <limits.h>
#include <stddef.h>
#include "world.h"
#include "../player.h"
#include "../networkhandler.h"
#include "../packet.h"
#include "../scripting/luabindings.h"

void ServerPlayerManager_Update(void) {
    /* v65.35: TEST IN GAME warp, consumed here on the server thread once
     * the client has finished loading (the request comes from the loading
     * screen, not from join - a join-time teleport hung the fill gate) */
    if (ParkourMapTakeWarpRequest()) {
        const PMap *pmap = ParkourMapActive();
        if (pmap && pmap->hasStart) {
            int ax, ay, az;
            ParkourMapAnchor(&ax, &ay, &az);
            Vector3 dest = { ax + pmap->start[0] + 0.5f,
                             ay + pmap->start[1] + 2.0f,
                             az + pmap->start[2] + 0.5f };
            for (int i = 0; i < WORLD_MAX_PLAYERS; i++)
                if (serverWorld.players[i] != NULL)
                    ServerPlayer_Teleport(serverWorld.players[i], dest);
        }
    }

    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        Player *player = serverWorld.players[i];
        if (player == NULL) continue;
        if (ServerNetwork_PlayerReadyForRemoval(player)) {
            ServerWorld_RemovePlayer(player);
            continue;
        }
        /* v65.38: kill plane - falling out of the Foundry course drops you
         * back on the START pad. Before this the runner fell out of the
         * pocket entirely and landed in the ordinary world below. */
        if (player->entityId >= 0) {
            Vector3 pp = serverWorld.entities[player->entityId].position;
            if (pp.x > -1392.0f && pp.x < -1008.0f &&
                pp.z >  1008.0f && pp.z <  1392.0f && pp.y < 110.0f) {
                Vector3 dest = { -1271.5f, 121.0f, 1200.5f };  /* built-in course gate */
                const PMap *pm = ParkourMapActive();
                if (pm && pm->hasStart) {
                    int ax, ay, az;
                    ParkourMapAnchor(&ax, &ay, &az);
                    dest = (Vector3){ ax + pm->start[0] + 0.5f,
                                      ay + pm->start[1] + 2.0f,
                                      az + pm->start[2] + 0.5f };
                }
                ServerPlayer_Teleport(player, dest);
            }
        }
        ServerPlayer_LoadChunks(player);
    }
}

void ServerPlayerManager_Shutdown(void) {
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] != NULL) ServerWorld_RemovePlayer(serverWorld.players[i]);
    }
}

void ServerWorld_AddPlayer(void *player) {
    Player *newPlayer = player;

    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] != NULL) continue;
        int entityId = ServerWorld_AddEntity(1, 0, (Vector3){0, 80, 0}, i);
        if (entityId < 0) return;
        newPlayer->entityId = entityId;
        serverWorld.players[i] = newPlayer;
        newPlayer->id = i;

        Entity localEntity = serverWorld.entities[newPlayer->entityId];
        localEntity.id = USHRT_MAX;
        ServerNetwork_Send(player, ServerPacket_CreateSpawnEntity(&localEntity));
        break;
    }

    if (newPlayer->entityId < 0) return;
    ServerEntities_Send(newPlayer);

    ServerWorld_SendMessage(TextFormat("%s joined the game!", newPlayer->name));
}

void ServerWorld_RemovePlayer(void *player) {
    Player *removedPlayer = player;
    ServerWorld_RemovePlayerFromChunks(removedPlayer);
    for (int i = 0; i < WORLD_MAX_PLAYERS; i++) {
        if (serverWorld.players[i] != removedPlayer) continue;
        LuaBindings_InvokePlayerLeave(i);
        serverWorld.players[i] = NULL;
        ServerWorld_RemoveEntity(removedPlayer->entityId);
        break;
    }
    ServerPlayer_Destroy(removedPlayer);
}
