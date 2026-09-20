/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_PLAYER_H
#define MIDLESS_SERVER_PLAYER_H

#include "raylib.h"
#include "blockdefinition.h"
#include "textureprotocol.h"

#define SERVER_MAX_PENDING_CHUNKS 8   /* v65.44 */

typedef struct Player {
    unsigned char id;
    int entityId;
    uint32_t textureSent[TEXTURE_LIMIT], textureRevision, textureOffset;
    int textureId;
    bool textureWaiting;
    double textureLastSend;
    uint64_t connectionId;
    void *peer;
    char *name;
    int drawDistance;
    bool isWeb;
    bool disconnected;
    int pendingPackets;
    /* v65.44: pipelined chunk generation requests. The old single
     * chunkRequestPending flag stalled ALL streaming (even sends of
     * already-generated chunks) while the loader worked on one chunk. */
    int pendingChunkCount;
    Vector3 pendingChunks[SERVER_MAX_PENDING_CHUNKS];
} Player;

Player *ServerPlayer_Create(void *peer, bool isWeb);
void ServerPlayer_Destroy(Player *player);
void ServerPlayer_UpdatePositionRotation(Player* player, Vector3 position, Vector3 rotation);
void ServerPlayer_LoadChunks(Player* player);
void ServerPlayer_Teleport(Player *player, Vector3 position);
void ServerPlayer_SendMessage(Player *player, const char *message);

// Send block definitions to this player.
void ServerPlayer_DefineBlock(Player *player, int id, const BlockDefinition *definition);
void ServerPlayer_RemoveBlockDefinition(Player *player, int id);

#endif
