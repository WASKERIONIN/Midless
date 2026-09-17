/* v65.28 pocketplay stubs - the raylib surface + the enet network edge.
 * Everything world-side is the REAL server implementation (world.c,
 * chunkmanager.c, entitymanager.c, playermanager.c, packet.c, player.c),
 * so the pocket hooks, fusion morph, routing and Foundry worldgen run
 * exactly as they do in the shipped dedicated/local server. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>
#include "raylib.h"
/* NOTE: no STB_DS_IMPLEMENTATION here - the real server world.c carries it */
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
double GetTime(void) {
    /* a real clock - the Lua hooks advance pocket_clock from it */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}
void TraceLog(int type, const char *fmt, ...) {
    (void)type;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}
void UnloadFileText(char *t) { (void)t; }
void UnloadDirectoryFiles(FilePathList l) { (void)l; }
void UnloadFileData(unsigned char *d) { (void)d; }
const char *TextFormat(const char *fmt, ...) { (void)fmt; return "stub"; }
void UnloadImage(Image img) { (void)img; }
unsigned int TextLength(const char *s) { return s ? strlen(s) : 0; }
const char *TextSubtext(const char *s, int start, int len) { (void)start; (void)len; return s; }

/* the enet edge - no sockets in the harness */
void ServerNetwork_Init(void) {}
void ServerNetwork_Send(void *playerData, unsigned char *packet) { (void)playerData; (void)packet; }
int ServerNetwork_PlayerReadyForRemoval(void *playerData) { (void)playerData; return 0; }
