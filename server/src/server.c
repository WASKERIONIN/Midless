/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#define ENET_IMPLEMENTATION
#include "enet.h"
#include "stb_ds.h"
#include "server.h"
#include "networkhandler.h"
#include "logger.h"   /* v65.7 bind log */

struct Player;
struct Player *ServerPlayer_Create(void *peer, bool isWeb);

#define MAX_CLIENTS 64

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

typedef struct ServerOutgoingPacket {
    ENetPeer *peer;
    unsigned char *data;
    int length;
} ServerOutgoingPacket;

static ServerOutgoingPacket *serverOutgoingPackets;
static pthread_mutex_t serverOutgoingMutex = PTHREAD_MUTEX_INITIALIZER;

static void Server_FlushOutgoing(ENetHost *server) {
    pthread_mutex_lock(&serverOutgoingMutex);
    ServerOutgoingPacket *packets = serverOutgoingPackets;
    serverOutgoingPackets = NULL;
    pthread_mutex_unlock(&serverOutgoingMutex);

    for (int i = 0; i < arrlen(packets); i++) {
        ENetPacket *packet = enet_packet_create(
            packets[i].data, packets[i].length, ENET_PACKET_FLAG_RELIABLE);
        if (packet != NULL && enet_peer_send(packets[i].peer, 0, packet) < 0) {
            enet_packet_destroy(packet);
        }
        free(packets[i].data);
    }
    if (arrlen(packets) > 0) enet_host_flush(server);
    arrfree(packets);
}

static void Server_ClearOutgoing(void) {
    pthread_mutex_lock(&serverOutgoingMutex);
    for (int i = 0; i < arrlen(serverOutgoingPackets); i++) {
        free(serverOutgoingPackets[i].data);
    }
    arrfree(serverOutgoingPackets);
    serverOutgoingPackets = NULL;
    pthread_mutex_unlock(&serverOutgoingMutex);
}

void *Server_Init(void *state) {
    
    enet_initialize();
    
    Server_Do((int*)state);
    
    enet_deinitialize();
    
    return NULL;
}

void Server_Do(int *state) {
    
    enet_initialize();
    ENetAddress address = {0};
    address.host = ENET_HOST_ANY;
    address.port = (unsigned short)serverConfig.port;

    /* v65.7: honour max_players from server.ini */
    ENetHost * server = enet_host_create(&address, serverConfig.maxPlayers, 1, 0, 0);
    char bindLog[160];
    snprintf(bindLog, sizeof(bindLog),
             "Listening on 0.0.0.0:%d as '%s' (max %d players). Give friends <your ip>:%d",
             serverConfig.port, serverConfig.name, serverConfig.maxPlayers, serverConfig.port);
    ServerLogger_Log(bindLog);
    ENetEvent event;
    
    while (*state != -1) {
        Server_FlushOutgoing(server);
        while (enet_host_service(server, &event, 5) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    event.peer->data = ServerPlayer_Create(event.peer, false);
                    ServerNetwork_Connect(event.peer->data);
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    ServerNetwork_Receive(event.peer->data, (unsigned char*)event.packet->data, event.packet->dataLength);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
#ifdef ENET_EVENT_TYPE_DISCONNECT_TIMEOUT
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
#endif
                    ServerNetwork_Disconnect(event.peer->data);
                    break;
                    
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
            Server_FlushOutgoing(server);
        }
        Server_FlushOutgoing(server);
    }
    

    Server_ClearOutgoing();
    enet_host_destroy(server);
    enet_deinitialize();
}

void Server_Send(void *peer, unsigned char* packet, int length) {
    if (peer == NULL || packet == NULL || length <= 0) return;

    ServerOutgoingPacket outgoing;
    outgoing.peer = peer;
    outgoing.data = malloc(length);
    if (outgoing.data == NULL) return;
    memcpy(outgoing.data, packet, length);
    outgoing.length = length;

    pthread_mutex_lock(&serverOutgoingMutex);
    arrput(serverOutgoingPackets, outgoing);
    pthread_mutex_unlock(&serverOutgoingMutex);
}
