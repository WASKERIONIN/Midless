#include "parkourmap.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#if !defined(OS_LINUX) && !defined(PLATFORM_WEB)
#else
#include <unistd.h>
#endif

void ParkourMapDir(char *out, int outLen) {
    snprintf(out, outLen, "%smaps/", GetApplicationDirectory());
}

int ParkourMapList(char names[][PMAP_NAME_LEN], int max) {
    char dir[512];
    ParkourMapDir(dir, sizeof(dir));
    if (!DirectoryExists(dir)) return 0;
    FilePathList files = LoadDirectoryFiles(dir);
    int count = 0;
    for (int i = 0; i < files.count && count < max; i++) {
        const char *file = GetFileName(files.paths[i]);
        if (!file || strcmp(GetFileExtension(file), "pmap") != 0) continue;
        snprintf(names[count], PMAP_NAME_LEN, "%.*s",
                 (int)(strlen(file) - 5), file);
        count++;
    }
    UnloadDirectoryFiles(files);
    return count;
}

static void MapPath(const char *name, char *out, int outLen) {
    char dir[512];
    ParkourMapDir(dir, sizeof(dir));
    snprintf(out, outLen, "%s%s.pmap", dir, name);
}

bool ParkourMapLoad(const char *name, PMap *out) {
    char path[600];
    MapPath(name, path, sizeof(path));
    if (!FileExists(path)) return false;
    char *text = LoadFileText(path);
    if (!text) return false;
    memset(out, 0, sizeof(*out));
    snprintf(out->name, PMAP_NAME_LEN, "%s", name);
    char *line = text;
    while (line && *line) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        int a, b, c, d, e, f, g;
        if (sscanf(line, "box %d %d %d %d %d %d %d", &a, &b, &c, &d, &e, &f, &g) == 7) {
            if (out->boxCount < PMAP_MAX_BOXES) {
                PMapBox *box = &out->boxes[out->boxCount++];
                box->x = a; box->y = b; box->z = c;
                box->hx = d; box->hy = e; box->hz = f; box->id = g;
            }
        } else if (sscanf(line, "gate %d %d %d", &a, &b, &c) == 3) {
            out->gate[0] = a; out->gate[1] = b; out->gate[2] = c;
            out->hasGate = true;
        } else if (sscanf(line, "start %d %d %d", &a, &b, &c) == 3) {
            out->start[0] = a; out->start[1] = b; out->start[2] = c;
            out->hasStart = true;
        } else if (sscanf(line, "finish %d %d %d", &a, &b, &c) == 3) {
            out->finish[0] = a; out->finish[1] = b; out->finish[2] = c;
            out->hasFinish = true;
        }
        line = end ? end + 1 : NULL;
    }
    UnloadFileText(text);
    return true;
}

bool ParkourMapSave(const PMap *map) {
    char dir[512];
    ParkourMapDir(dir, sizeof(dir));
    if (!DirectoryExists(dir)) {
#if defined(OS_LINUX) || defined(PLATFORM_WEB)
        mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#else
        mkdir(dir);
#endif
        if (!DirectoryExists(dir)) return false;
    }
    char path[600];
    MapPath(map->name, path, sizeof(path));
    char *body = (char *)MemAlloc(120 * 1024);
    if (!body) return false;
    int n = 0;
    if (map->hasGate)
        n += snprintf(body + n, 120 * 1024 - n, "gate %d %d %d\n",
                      map->gate[0], map->gate[1], map->gate[2]);
    if (map->hasStart)
        n += snprintf(body + n, 120 * 1024 - n, "start %d %d %d\n",
                      map->start[0], map->start[1], map->start[2]);
    if (map->hasFinish)
        n += snprintf(body + n, 120 * 1024 - n, "finish %d %d %d\n",
                      map->finish[0], map->finish[1], map->finish[2]);
    for (int i = 0; i < map->boxCount; i++) {
        const PMapBox *b = &map->boxes[i];
        n += snprintf(body + n, 120 * 1024 - n, "box %d %d %d %d %d %d %d\n",
                      b->x, b->y, b->z, b->hx, b->hy, b->hz, b->id);
    }
    bool ok = SaveFileText(path, body);
    MemFree(body);
    return ok;
}

bool ParkourMapDelete(const char *name) {
    char path[600];
    MapPath(name, path, sizeof(path));
    return FileExists(path) && remove(path) == 0;
}

static PMap activeMap;
static bool activeValid = false;
static char activeName[PMAP_NAME_LEN] = "";

void ParkourMapSetSession(const PMap *map) {
    if (map) {
        activeMap = *map;
        activeValid = true;
        snprintf(activeName, PMAP_NAME_LEN, "%s", map->name);
    } else {
        activeValid = false;
        activeName[0] = 0;
    }
}

void ParkourMapSetActive(const char *name) {
    activeValid = false;
    activeName[0] = 0;
    if (!name || !name[0] || strcmp(name, "default") == 0) return;
    if (ParkourMapLoad(name, &activeMap)) {
        activeValid = true;
        snprintf(activeName, PMAP_NAME_LEN, "%s", name);
    }
}

const char *ParkourMapActiveName(void) { return activeName; }
const PMap *ParkourMapActive(void) { return activeValid ? &activeMap : NULL; }

void ParkourMapAnchor(int *x, int *y, int *z) {
    /* the second pocket zone centre, west-shifted like the built-in
     * course start court, floor top at 118 */
    *x = -1200 - 96;
    *y = 118;
    *z = 1200 - 96;
}
