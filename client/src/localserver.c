#if defined(OS_WINDOWS)
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER
#endif

#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#if defined(OS_WINDOWS)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif
#include "raylib.h"
#include "localserver.h"
#include "i18n.h"
#include "gui/chat.h"
#include "../../server/src/server.h"      /* v65.7: ServerConfig */
#include "../../server/src/world/world.h"
#include "../../server/src/player.h"
#include "../../server/src/networkhandler.h"
#include "../../server/src/scripting/luaengine.h"
#include "../../server/src/scripting/luabindings.h"

extern int networkConnectedToServer;
extern void (*networkClientSend)(unsigned char *, int);
void Network_Init(void);
void Network_Connect(void);
void Network_Receive(unsigned char *data, int dataLength);
void Network_ClearQueue(void);
void World_Clear(void);

static Player *localPlayer;
static bool localServerRunning;
static bool localServerThreadCreated;
static pthread_t localServerThread;
static pthread_mutex_t localServerStateMutex = PTHREAD_MUTEX_INITIALIZER;

static void LocalServer_SetRunning(bool running) {
    pthread_mutex_lock(&localServerStateMutex);
    localServerRunning = running;
    pthread_mutex_unlock(&localServerStateMutex);
}

bool LocalServer_IsRunning(void) {
    pthread_mutex_lock(&localServerStateMutex);
    bool running = localServerRunning;
    pthread_mutex_unlock(&localServerStateMutex);
    return running;
}

static void *LocalServer_Run(void *unused) {
    (void)unused;

    LuaBindings_InvokeReady();

    while (LocalServer_IsRunning()) {
        ServerNetwork_ProcessIncomingPackets();
        ServerWorld_Update();

        usleep(1000);
    }

    return NULL;
}

void Server_Send(void *peer, unsigned char *packet, int length) {
    (void)peer;
    Network_Receive(packet, length);
}

static void LocalServer_Send(unsigned char *packet, int length) {
    ServerNetwork_Receive(localPlayer, packet, length);
}

/* ---- v65.7 host config + address helpers ---- */
static char hostLocalIp[46] = "127.0.0.1";

static void LocalServer_ConfigPath(char *out, int outLen) {
    const char *dir = GetApplicationDirectory();
    snprintf(out, (size_t)outLen, "%sserver.ini", dir ? dir : "");
}

/* the LAN address of this machine: a UDP "connect" only picks the source
 * address of the outgoing route - nothing is actually sent */
static void LocalServer_DetectLocalIp(void) {
#if defined(OS_WINDOWS)
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return;
    struct sockaddr_in dst; memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET; dst.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
    if (connect(s, (struct sockaddr *)&dst, sizeof(dst)) == 0) {
        struct sockaddr_in me; memset(&me, 0, sizeof(me));
        int len = sizeof(me);
        if (getsockname(s, (struct sockaddr *)&me, &len) == 0)
            inet_ntop(AF_INET, &me.sin_addr, hostLocalIp, sizeof(hostLocalIp));
    }
    closesocket(s);
#else
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return;
    struct sockaddr_in dst; memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET; dst.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
    if (connect(s, (struct sockaddr *)&dst, sizeof(dst)) == 0) {
        struct sockaddr_in me; memset(&me, 0, sizeof(me));
        socklen_t len = sizeof(me);
        if (getsockname(s, (struct sockaddr *)&me, &len) == 0)
            inet_ntop(AF_INET, &me.sin_addr, hostLocalIp, sizeof(hostLocalIp));
    }
    close(s);
#endif
}

void LocalServer_ReadHostConfig(char *name, int nameLen, int *port, int *maxPlayers) {
    char path[256];
    LocalServer_ConfigPath(path, sizeof(path));
    ServerConfig_WriteTemplate(path);
    ServerConfig_Load(path);
    const ServerConfig *cfg = ServerConfig_Get();
    if (name && nameLen > 0) snprintf(name, (size_t)nameLen, "%s", cfg->name);
    if (port) *port = cfg->port;
    if (maxPlayers) *maxPlayers = cfg->maxPlayers;
}

void LocalServer_WriteHostConfig(const char *portStr, const char *maxStr, const char *name) {
    char path[256];
    LocalServer_ConfigPath(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f,
            "# Midless Cosmic Edition server config\n"
            "# Friends type  <your address>:%s  into the Login screen to join.\n"
            "# LAN: the address shown in the host panel (F6 in game).\n"
            "# Internet: forward this port on your router to this PC.\n"
            "port=%s\n"
            "max_players=%s\n"
            "name=%s\n",
            portStr, portStr, maxStr, name);
        fclose(f);
    }
    ServerConfig_Load(path);
}

const char *LocalServer_GetLocalIp(void) { return hostLocalIp; }
int LocalServer_GetPort(void) { return ServerConfig_Get()->port; }
int LocalServer_GetMaxPlayers(void) { return ServerConfig_Get()->maxPlayers; }
const char *LocalServer_GetName(void) { return ServerConfig_Get()->name; }
int LocalServer_GetPlayerCount(void) {
    /* remote peers plus the host itself */
    return ServerNetwork_GetPlayerCount() + 1;
}

bool LocalServer_Start(void) {
    if (LocalServer_IsRunning()) return true;

    /* v65.7: host config + the address friends will type */
    {
        char path[256];
        LocalServer_ConfigPath(path, sizeof(path));
        ServerConfig_WriteTemplate(path);
        ServerConfig_Load(path);
        LocalServer_DetectLocalIp();
    }

    Lua_Init();
    LuaBindings_Init();
    ServerWorld_Init();
    ServerNetwork_Init();
    if (!Lua_Run()) {
        ServerNetwork_Shutdown();
        ServerWorld_Shutdown();
        LuaBindings_Shutdown();
        Lua_Stop();
        return false;
    }
    localPlayer = ServerPlayer_Create(NULL, false);
    if (localPlayer == NULL) {
        ServerWorld_Shutdown();
        LuaBindings_Shutdown();
        Lua_Stop();
        return false;
    }
    localPlayer->peer = localPlayer;

    networkClientSend = LocalServer_Send;
    networkConnectedToServer = true;
    Network_Init();
    LocalServer_SetRunning(true);
    if (pthread_create(&localServerThread, NULL, LocalServer_Run, NULL) != 0) {
        LocalServer_SetRunning(false);
        networkConnectedToServer = false;
        ServerNetwork_Shutdown();
        ServerWorld_Shutdown();
        LuaBindings_Shutdown();
        Lua_Stop();
        localPlayer = NULL;
        return false;
    }
    localServerThreadCreated = true;
    Network_Connect();
    Chat_AddLine(TextFormat(Tr("Server '%s' listening on %s:%d - send this address to friends. F6 - host panel."),
                            LocalServer_GetName(), hostLocalIp, LocalServer_GetPort()));
    return true;
}

void LocalServer_WipeWorld(bool keepSeed) {
    DIR *dir = opendir("world");
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (keepSeed && strcmp(entry->d_name, "seed.dat") == 0) continue;
        char path[512];
        snprintf(path, sizeof(path), "world/%s", entry->d_name);
        remove(path);
    }
    closedir(dir);
}

void LocalServer_Stop(void) {
    if (!LocalServer_IsRunning()) return;
    LocalServer_SetRunning(false);
    if (localServerThreadCreated) {
        pthread_join(localServerThread, NULL);
        localServerThreadCreated = false;
    }
    ServerNetwork_Shutdown();
    ServerWorld_Shutdown();
    LuaBindings_Shutdown();
    Lua_Stop();
    localPlayer = NULL;
    World_Clear();
    Network_ClearQueue();
    networkConnectedToServer = false;
}
