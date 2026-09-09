/* Minimal raylib stubs so the server-core sources can run headless on Linux. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#define _DEFAULT_SOURCE 1
#include <time.h>
#include "raylib.h"

void *MemAlloc(unsigned int size) { return malloc((size_t)size); }
void MemFree(void *ptr) { free(ptr); }

bool FileExists(const char *fileName) {
    struct stat st;
    return stat(fileName, &st) == 0;
}

char *LoadFileText(const char *fileName) {
    FILE *f = fopen(fileName, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)calloc(1, (size_t)size + 1);
    if (size > 0) fread(text, 1, (size_t)size, f);
    fclose(f);
    return text;
}

bool SaveFileText(const char *fileName, char *text) {
    FILE *f = fopen(fileName, "w");
    if (!f) return false;
    size_t len = strlen(text);
    fwrite(text, 1, len, f);
    fclose(f);
    return true;
}

unsigned char *LoadFileData(const char *fileName, unsigned int *bytesRead) {
    *bytesRead = 0;
    FILE *f = fopen(fileName, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc((size_t)size + 1);
    if (size > 0) fread(data, 1, (size_t)size, f);
    fclose(f);
    *bytesRead = (unsigned int)size;
    return data;
}

bool SaveFileData(const char *fileName, void *data, unsigned int bytesToWrite) {
    FILE *f = fopen(fileName, "wb");
    if (!f) return false;
    fwrite(data, 1, (size_t)bytesToWrite, f);
    fclose(f);
    return true;
}

Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize) {
    (void)fileType; (void)fileData; (void)dataSize;
    Image img = { 0 };
    img.width = 1; img.height = 1; img.mipmaps = 1; img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    img.data = calloc(4, 1);
    return img;
}

void UnloadImage(Image image) { free(image.data); }

FilePathList LoadDirectoryFiles(const char *dirPath) {
    FilePathList list = { 0 };
    DIR *dir = opendir(dirPath);
    if (!dir) return list;
    char **paths = NULL;
    int count = 0, cap = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            paths = realloc(paths, (size_t)cap * sizeof(char *));
        }
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dirPath, entry->d_name);
        paths[count++] = strdup(full);
    }
    closedir(dir);
    list.paths = (const char **)paths;
    list.count = count;
    list.capacity = cap;
    return list;
}

void UnloadDirectoryFiles(FilePathList files) {
    for (unsigned int i = 0; i < files.count; i++) free((void *)files.paths[i]);
    free((void *)files.paths);
}

double GetTime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void TraceLog(int logType, const char *text, ...) {
    (void)logType;
    va_list args;
    va_start(args, text);
    vfprintf(stderr, text, args);
    fprintf(stderr, "\n");
    va_end(args);
}

const char *TextFormat(const char *text, ...) {
    static char buffer[4][2048];
    static int index = 0;
    char *buf = buffer[index & 3];
    index++;
    va_list args;
    va_start(args, text);
    vsnprintf(buf, 2048, text, args);
    va_end(args);
    return buf;
}

unsigned int TextLength(const char *text) { return (unsigned int)strlen(text); }

const char *TextSubtext(const char *text, int position, int length) {
    static char buffer[2048];
    int textLength = (int)strlen(text);
    if (position > textLength) position = textLength;
    if (position + length > textLength) length = textLength - position;
    memcpy(buffer, text + position, (size_t)length);
    buffer[length] = '\0';
    return buffer;
}

float Vector3Distance(Vector3 a, Vector3 b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

int Vector3Equals(Vector3 a, Vector3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

float Vector3LengthSqr(Vector3 v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

Vector3 Vector3Multiply(Vector3 a, Vector3 b) {
    return (Vector3){ a.x * b.x, a.y * b.y, a.z * b.z };
}

Vector3 Vector3Subtract(Vector3 a, Vector3 b) {
    return (Vector3){ a.x - b.x, a.y - b.y, a.z - b.z };
}

void Server_Send(void *peer, unsigned char *packet, int length) {
    (void)peer; (void)packet; (void)length;
}

/* stb_ds implementation lives in its own TU (see build script). */

void UnloadFileData(unsigned char *data) { free(data); }

void UnloadFileText(char *text) { free(text); }
