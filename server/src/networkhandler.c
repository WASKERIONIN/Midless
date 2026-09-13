/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "stb_ds.h"
#include "raylib.h"
#include "player.h"
#include "networkhandler.h"

#include "packet.h"
#include "server.h"
#include "serverwss.h"
#include "world/world.h"
#include "logger.h"
#include "world/textures.h"

/* v65.7: server.ini config lives HERE (not server.c) because the client
 * build compiles networkhandler.c but not server.c - the in-game host
 * (localserver.c) and the dedicated server.exe share these functions. */
#define MAX_CLIENTS MIDLESS_MAX_CLIENTS

/* v65.7: server.ini config - port / max players / name, shared by the
 * dedicated server.exe and the in-game host (localserver.c loads the
 * same file before spawning the embedded server thread). */
static ServerConfig serverConfig = { 25565, 8, "Midless Cosmic Server" };

const ServerConfig *ServerConfig_Get(void) { return &serverConfig; }

void ServerConfig_WriteTemplate(const char *path) {
    FILE *probe = fopen(path, "r");
    if (probe) { fclose(probe); return; }
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
        "# Midless Cosmic Edition server config\n"
        "# Friends type  <your address>:%d  into the Login screen to join.\n"
        "# LAN: the address shown in the host panel (F6 in game).\n"
        "# Internet: forward this port on your router to this PC.\n"
        "port=%d\n"
        "# 1..%d\n"
        "max_players=%d\n"
        "name=%s\n",
        serverConfig.port, serverConfig.port, MAX_CLIENTS,
        serverConfig.maxPlayers, serverConfig.name);
    fclose(f);
}

void ServerConfig_Load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int v = 0;
        if (sscanf(line, "port=%d", &v) == 1) {
            if (v >= 1 && v <= 65535) serverConfig.port = v;
        } else if (sscanf(line, "max_players=%d", &v) == 1) {
            if (v >= 1 && v <= MAX_CLIENTS) serverConfig.maxPlayers = v;
        } else if (strncmp(line, "name=", 5) == 0) {
            char *nl = line + 5;
            size_t n = strcspn(nl, "\r\n");
            if (n > 0 && n < sizeof(serverConfig.name)) {
                memcpy(serverConfig.name, nl, n);
                serverConfig.name[n] = 0;
            }
        }
    }
    fclose(f);
}


PacketHandlerEntry serverPacketHandlers[256];
int serverPacketHandlerCount = 0;
pthread_mutex_t serverNetworkMutex;
IncomingPacket *serverIncomingPackets = NULL;

#define SERVER_MAX_QUEUED_PACKETS 256 * 64
#define SERVER_MAX_QUEUED_PACKETS_PER_PLAYER 256

static const int serverIncomingPacketLengths[] = {
    67, 
    14, 
    16,
    65, 
    2,
    2,
    TEXTURE_ACK_SIZE,
    2 // held block
};

void ServerNetwork_Init(void) {
    pthread_mutex_init(&serverNetworkMutex, NULL);
    serverPacketHandlerCount = 0;
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleIdentification};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleSetBlock};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandlePlayerPosition};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleMessage};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleSetDrawDistance};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandlePlayerClick};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerTextures_HandleAck};
    serverPacketHandlers[serverPacketHandlerCount++] = (PacketHandlerEntry) {&ServerPacket_HandleHeldBlock};
}

void ServerNetwork_Shutdown(void) {
    pthread_mutex_lock(&serverNetworkMutex);
    for (int i = 0; i < arrlen(serverIncomingPackets); i++) {
        Player *player = serverIncomingPackets[i].player;
        if (player != NULL && player->pendingPackets > 0) player->pendingPackets--;
        free(serverIncomingPackets[i].data);
    }
    arrfree(serverIncomingPackets);
    serverIncomingPackets = NULL;
    pthread_mutex_unlock(&serverNetworkMutex);
    pthread_mutex_destroy(&serverNetworkMutex);
}

/* v65.7: how many peers are attached - the host panel shows N/M */
static int serverConnectedPlayers = 0;

int ServerNetwork_GetPlayerCount(void) { return serverConnectedPlayers; }

void ServerNetwork_Connect(void *playerData) {
    (void)playerData;
    pthread_mutex_lock(&serverNetworkMutex);
    serverConnectedPlayers++;
    pthread_mutex_unlock(&serverNetworkMutex);
}

void ServerNetwork_Disconnect(void *playerData) {
    Player *player = (Player*)playerData;
    if (player == NULL) return;

    pthread_mutex_lock(&serverNetworkMutex);

    if (player->disconnected) {
        pthread_mutex_unlock(&serverNetworkMutex);
        return;
    }
    if (serverConnectedPlayers > 0) serverConnectedPlayers--;

    ServerLogger_Log(TextFormat("%s disconnected.\n",
        player->name != NULL ? player->name : "Unidentified player"));
    player->disconnected = true;

    for (int i = arrlen(serverIncomingPackets) - 1; i >= 0; i--) {
        if (serverIncomingPackets[i].player != player) continue;
        free(serverIncomingPackets[i].data);
        arrdel(serverIncomingPackets, i);
        if (player->pendingPackets > 0) player->pendingPackets--;
    }

    bool destroyUnidentified = player->name == NULL && player->pendingPackets == 0;
   
    pthread_mutex_unlock(&serverNetworkMutex);

    if (destroyUnidentified) ServerPlayer_Destroy(player);
}

int ServerNetwork_PlayerReadyForRemoval(void *playerData) {
    Player *player = playerData;
    if (player == NULL) return 0;

    pthread_mutex_lock(&serverNetworkMutex);
    int ready = player->disconnected && player->pendingPackets == 0;
    pthread_mutex_unlock(&serverNetworkMutex);

    return ready;
}

void ServerNetwork_ProcessIncomingPackets(void) {
    while (true) {
        IncomingPacket packet = {0};

        pthread_mutex_lock(&serverNetworkMutex);
        if (arrlen(serverIncomingPackets) > 0) {
            packet = serverIncomingPackets[0];
            arrdel(serverIncomingPackets, 0);
        }
        pthread_mutex_unlock(&serverNetworkMutex);

        if (packet.data == NULL) return;

        serverPacketPlayer = (Player*)packet.player;
        serverPacketData = packet.data;
        serverPacketDataLength = packet.length;
        serverPacketReaderIndex = 1;
        if (!serverPacketPlayer->disconnected &&
            (serverPacketData[0] == 0 || serverPacketPlayer->entityId >= 0)) {
            (*serverPacketHandlers[serverPacketData[0]].handler)();
        }
        free(packet.data);

        pthread_mutex_lock(&serverNetworkMutex);

        if (serverPacketPlayer->pendingPackets > 0) serverPacketPlayer->pendingPackets--;

        bool destroyUnidentified = serverPacketPlayer->disconnected &&
            serverPacketPlayer->name == NULL && serverPacketPlayer->pendingPackets == 0;

        pthread_mutex_unlock(&serverNetworkMutex);

        if (destroyUnidentified) ServerPlayer_Destroy(serverPacketPlayer);
    }
}

void ServerNetwork_Receive(void *playerData, unsigned char* data, int dataLength) {
    if (playerData == NULL || data == NULL || dataLength < 1) return;

    unsigned char opcode = data[0];
    if (opcode >= serverPacketHandlerCount ||
        dataLength != serverIncomingPacketLengths[opcode]) return;

    pthread_mutex_lock(&serverNetworkMutex);

    Player *player = playerData;
    if (player->disconnected ||
        arrlen(serverIncomingPackets) >= SERVER_MAX_QUEUED_PACKETS ||
        player->pendingPackets >= SERVER_MAX_QUEUED_PACKETS_PER_PLAYER) {
        pthread_mutex_unlock(&serverNetworkMutex);
        return;
    }

    IncomingPacket packet;
    packet.data = malloc(dataLength);
    if (packet.data == NULL) {
        pthread_mutex_unlock(&serverNetworkMutex);
        return;
    }
    memcpy(packet.data, data, dataLength);
    packet.player = playerData;
    packet.length = dataLength;
    arrput(serverIncomingPackets, packet);
    player->pendingPackets++;

    pthread_mutex_unlock(&serverNetworkMutex);
}

void ServerNetwork_Send(void *playerData, unsigned char* packet) {

    if(packet == NULL) return;

    Player *player = (Player*)playerData;
    int packetLength = ServerPacket_GetLength(packet[0]);
    if (packetLength == 0) packetLength = serverPacketLastDynamicLength;

    if(player->isWeb == false) {
        Server_Send(player->peer, packet, packetLength);
    } else {
        #if defined(SERVER_WEB_SUPPORT)
        ServerWss_Send(player->peer, packet, packetLength);
        #endif
    }

    MemFree(packet);

}
