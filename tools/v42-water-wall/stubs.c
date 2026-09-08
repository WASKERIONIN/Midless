#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "raylib.h"
void *MemAlloc(unsigned int size) { return calloc(size ? size : 1, 1); }
void MemFree(void *p) { free(p); }
void TraceLog(int level, const char *text, ...) { (void)level; (void)text; }
bool FileExists(const char *f) { (void)f; return false; }
unsigned char *LoadFileData(const char *f, unsigned int *n) { (void)f; *n = 0; return NULL; }
void UnloadFileData(unsigned char *d) { free(d); }
bool SaveFileData(const char *f, void *d, unsigned int n) { (void)f; (void)d; (void)n; return true; }
