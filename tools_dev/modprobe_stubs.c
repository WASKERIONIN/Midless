/* v62 mod probe stubs - throwaway validation tool, not shipped. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "raylib.h"
#include "stb_ds.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

void *MemAlloc(unsigned int size) { return calloc(1, size); }
void MemFree(void *p) { free(p); }
unsigned char *LoadFileData(const char *path, unsigned int *out) { (void)path; *out = 0; return NULL; }
bool SaveFileData(const char *path, void *data, unsigned int n) { (void)path; (void)data; (void)n; return false; }
char *LoadFileText(const char *path) { (void)path; return NULL; }
bool SaveFileText(const char *path, char *text) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fputs(text ? text : "", f);
    fclose(f);
    return true;
}
bool FileExists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fclose(f);
    return true;
}
const char *GetFileExtension(const char *path) { (void)path; return ""; }
const char *GetFileName(const char *path) { return path; }
FilePathList LoadDirectoryFiles(const char *path) {
    (void)path;
    FilePathList l = {0};
    return l;
}
Image LoadImageFromMemory(const char *type, const unsigned char *data, int n) {
    (void)type; (void)data; (void)n;
    Image img = {0};
    return img;
}
double GetTime(void) { return 0.0; }

/* server world/network surface used by lua bindings */
typedef struct ServerWorld ServerWorld;
#include "world/world.h"
World serverWorld;
static char dummy_players[64];
__attribute__((constructor)) static void modprobe_boot(void) {
    memset(&serverWorld, 0, sizeof(serverWorld));
    serverWorld.players = (Player **)dummy_players;
}

/* server world/network surface used by lua bindings - no-ops */
void ServerNetwork_Send(Player *p, unsigned char *packet, int len, void *extra) { (void)p; (void)packet; (void)len; (void)extra; }
void ServerNetwork_PlayerReadyForRemoval(Player *p) { (void)p; }
void ServerWorld_Broadcast(unsigned char *packet) { (void)packet; }
void ServerWorld_BroadcastExcluding(unsigned char *packet, int excludedPlayerId) { (void)packet; (void)excludedPlayerId; }
void ServerWorld_DefineBlock(int id, const BlockDefinition *definition) {
    if (id < 0 || id > 255 || !definition) return;
    serverWorld.blockDefinitions[id] = *definition;
    serverWorld.hasBlockDefinition[id] = true;
}
bool ServerWorld_DefineEntityModel(int id, const ModelDefinition *definition) { (void)id; (void)definition; return true; }
int ServerWorld_GetBlock(Vector3 p) { (void)p; return 0; }
Chunk *ServerWorld_GetChunkAt(Vector3 p) { (void)p; return NULL; }
bool ServerWorld_IsBlockDefined(int id) { (void)id; return true; }
void ServerWorld_SendMessage(const char *message) { (void)message; }
bool ServerWorld_SetEntityModel(int entityId, int modelId) { (void)entityId; (void)modelId; return true; }
void ServerWorld_RemoveEntityModel(int id) { (void)id; }
void ServerWorld_SetBlock(Vector3 p, int id, bool b1, bool b2, bool b3) { (void)p; (void)id; (void)b1; (void)b2; (void)b3; }
void ServerWorld_SendEntityModels(Player *player) { (void)player; }
void ServerWorld_SendBlockDefinitions(Player *player) { (void)player; }
bool ServerWorld_QueueChunk(Vector3 p) { (void)p; return false; }
void ServerWorld_RemovePlayerFromChunks(Player *p) { (void)p; }
void TraceLog(int type, const char *fmt, ...) { (void)type; (void)fmt; }
void UnloadFileText(char *t) { (void)t; }
void UnloadDirectoryFiles(FilePathList l) { (void)l; }
void UnloadFileData(unsigned char *d) { (void)d; }
const char *TextFormat(const char *fmt, ...) { (void)fmt; return "stub"; }
void UnloadImage(Image img) { (void)img; }
unsigned int TextLength(const char *s) { return s ? strlen(s) : 0; }
const char *TextSubtext(const char *s, int start, int len) { (void)start; (void)len; return s; }
