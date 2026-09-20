/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "raylib.h"
#include "raymath.h"
#include "player.h"
#include "world/world.h"
#include "world/chunk/chunk.h"
#include "networkhandler.h"
#include "packet.h"

static uint64_t nextConnectionId;

Player *ServerPlayer_Create(void *peer, bool isWeb) {
    Player *player = MemAlloc(sizeof(*player));
    if (player == NULL) return NULL;

    *player = (Player){0};
    player->entityId = -1;
    player->connectionId = ++nextConnectionId;
    player->peer = peer;
    player->drawDistance = 3;
    player->isWeb = isWeb;
    return player;
}

void ServerPlayer_Destroy(Player *player) {
    if (player == NULL) return;
    MemFree(player->name);
    MemFree(player);
}

void ServerPlayer_DefineBlock(Player *player, int id, const BlockDefinition *definition) {
    if (!player || player->disconnected) return;
    unsigned char *packet = ServerPacket_CreateDefineBlock(id, definition);
    if (!packet) return;
    ServerNetwork_Send(player, packet);
}

void ServerPlayer_RemoveBlockDefinition(Player *player, int id) {
    if (!player || player->disconnected) return;
    unsigned char *packet = ServerPacket_CreateRemoveBlockDefinition(id);
    if (!packet) return;
    ServerNetwork_Send(player, packet);
}

void ServerPlayer_UpdatePositionRotation(Player* player, Vector3 position, Vector3 rotation) {
    ServerWorld_TeleportEntity(player->entityId, position, rotation);
}

void ServerPlayer_Teleport(Player *player, Vector3 position) {
    if (player->entityId < 0) return;
    Entity *entity = &serverWorld.entities[player->entityId];
    ServerWorld_TeleportEntity(player->entityId, position, entity->rotation);
    Entity localEntity = *entity;
    localEntity.id = USHRT_MAX;
    Vector3 localPosition = {position.x - 0.5f, position.y, position.z - 0.5f};
    ServerNetwork_Send(player, ServerPacket_CreateTeleportEntity(&localEntity, localPosition, entity->rotation));
}

/* v65.44: is this position already requested from the loader? */
static bool ServerPlayer_PendingContains(Player *player, Vector3 pos) {
    for (int i = 0; i < player->pendingChunkCount; i++)
        if (Vector3Equals(player->pendingChunks[i], pos)) return true;
    return false;
}

void ServerPlayer_LoadChunks(Player* player) {

    Entity entity = serverWorld.entities[player->entityId];
    double loadDeadline = GetTime() + 0.008;

    Vector3 playerChunkPos = (Vector3) {(int)floor(entity.position.x / CHUNK_SIZE_X), (int)floor(entity.position.y / CHUNK_SIZE_Y), (int)floor(entity.position.z / CHUNK_SIZE_Z)};

    int loadingHeight = fmin(player->drawDistance, 4);
    while (true) {
        bool foundChunk = false;
        float closestDistanceSquared = INFINITY;
        Vector3 closestPosition = {0};

        for (int y = -loadingHeight; y <= loadingHeight; y++) {
            for (int x = -player->drawDistance; x <= player->drawDistance; x++) {
                for (int z = -player->drawDistance; z <= player->drawDistance; z++) {
                    float distanceSquared = (float)(x*x + y*y + z*z);
                    float loadingRadius = player->drawDistance + 3;
                    if (distanceSquared >= loadingRadius * loadingRadius ||
                        distanceSquared >= closestDistanceSquared) continue;

                    Vector3 chunkPos = {
                        playerChunkPos.x + x,
                        playerChunkPos.y + y,
                        playerChunkPos.z + z
                    };
                    /* v65.44: already asked the loader for this one */
                    if (ServerPlayer_PendingContains(player, chunkPos)) continue;
                    Chunk *chunk = ServerWorld_GetChunkAt(chunkPos);
                    if (chunk != NULL && ServerChunk_PlayerInChunk(chunk, player)) continue;

                    foundChunk = true;
                    closestDistanceSquared = distanceSquared;
                    closestPosition = chunkPos;
                }
            }
        }

        if (!foundChunk) return;

        Chunk *chunk = ServerWorld_GetChunkAt(closestPosition);
        if (chunk == NULL) {
            if (player->pendingChunkCount >= SERVER_MAX_PENDING_CHUNKS) return;
            ServerWorld_QueueChunk(closestPosition);
            player->pendingChunks[player->pendingChunkCount++] = closestPosition;
            /* v65.44: pipeline - immediately look for the next-nearest
             * missing chunk instead of idling until this one lands; the
             * loader thread stays fed and the generate->compress->send
             * conveyor no longer runs one chunk at a time */
            continue;
        }
        ServerChunk_AddPlayer(chunk, player);

        int compressedLength = 0;
        unsigned short *compressedChunk = ServerChunk_CreateCompressedData(chunk, &compressedLength);
        ServerNetwork_Send(player, ServerPacket_CreateLoadChunk(
            compressedChunk, compressedLength, closestPosition, chunk->skyMask));
        MemFree(compressedChunk);

        if (GetTime() >= loadDeadline) return;
    }
}

void ServerPlayer_SendMessage(Player *player, const char *message) {
    if (!player || player->disconnected) return;
    int length = TextLength(message);
    int parts = length > 0 ? (length + 63) / 64 : 1;
    for (int i = 0; i < parts; i++) {
        const char *part = TextSubtext(message, i * 64, 64);
        ServerNetwork_Send(player, i == 0 ? ServerPacket_CreateMessage(part)
                                         : ServerPacket_CreateMessageContinuation(part));
    }
}
