/* Minimal stand-ins for the raylib/enet layer so the *real* Midless worldgen
 * translation units can be compiled and executed headlessly.  Nothing here
 * re-implements world logic: it only supplies the few engine entry points the
 * worldgen code calls (file IO, logging, allocation) plus the globals it links
 * against. */
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#define LUA_IMPL
#include "minilua.h"
#define FNL_IMPL
#include "FastNoiseLite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "world.h"

World serverWorld;
lua_State *L;
int luaRunning = 0;

void TraceLog(int logLevel, const char *text, ...) {
    va_list args;
    va_start(args, text);
    fprintf(stderr, "[log %d] ", logLevel);
    vfprintf(stderr, text, args);
    fputc('\n', stderr);
    va_end(args);
}
void SetTraceLogLevel(int level) { (void)level; }
void SetTraceLogCallback(TraceLogCallback cb) { (void)cb; }

void *MemAlloc(unsigned int size) { return malloc(size); }
void *MemRealloc(void *ptr, unsigned int size) { return realloc(ptr, size); }
void MemFree(void *ptr) { free(ptr); }

bool FileExists(const char *fileName) {
    FILE *f = fopen(fileName, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}
unsigned char *LoadFileData(const char *fileName, unsigned int *bytesRead) {
    FILE *f = fopen(fileName, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buffer = malloc(size ? size : 1);
    size_t got = size ? fread(buffer, 1, size, f) : 0;
    fclose(f);
    *bytesRead = (unsigned int)got;
    return buffer;
}
bool SaveFileData(const char *fileName, void *data, unsigned int bytesToWrite) {
    FILE *f = fopen(fileName, "wb");
    if (!f) return false;
    size_t wrote = fwrite(data, 1, bytesToWrite, f);
    fclose(f);
    return wrote == bytesToWrite;
}
void UnloadFileData(unsigned char *data) { free(data); }
char *LoadFileText(const char *fileName) {
    unsigned int length = 0;
    unsigned char *data = LoadFileData(fileName, &length);
    if (!data) return NULL;
    char *text = malloc(length + 1);
    memcpy(text, data, length);
    text[length] = 0;
    UnloadFileData(data);
    return text;
}
bool SaveFileText(const char *fileName, char *text) {
    return SaveFileData(fileName, text, (unsigned int)strlen(text));
}
void UnloadFileText(char *text) { free(text); }
double GetTime(void) { return 0; }
