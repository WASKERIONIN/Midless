/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_SERVER_TRANSPORT_H
#define MIDLESS_SERVER_TRANSPORT_H

void *Server_Init(void *state);
void Server_Do(int *state);
void Server_Send(void *peer, unsigned char* packet, int length);

/* v65.7: server config (server.ini next to the executable). The same
 * file configures the dedicated server.exe AND the in-game host, so a
 * player who edits it once gets the same server either way. */
typedef struct ServerConfig {
    int port;             /* 1..65535, default 25565 */
    int maxPlayers;       /* 1..64, default 8 */
    char name[64];        /* shown in the host panel and logs */
} ServerConfig;
const ServerConfig *ServerConfig_Get(void);
void ServerConfig_Load(const char *path);          /* missing keys keep defaults */
void ServerConfig_WriteTemplate(const char *path); /* only when absent */

#endif