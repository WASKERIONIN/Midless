#include "parkourmap.h"
#include "raylib.h"   /* GetApplicationDirectory only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

/* v65.34: the file layer is native stdio/dirent - the previous raylib
 * SaveFileText/LoadDirectoryFiles path silently failed on some Windows
 * setups (empty maps/ folder, empty picker list). Every write is now
 * verified on disk and every failure records a human-readable reason. */

static char lastError[256] = "";
const char *ParkourMapLastError(void) { return lastError; }

static bool DirExistsRaw(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFDIR);
}

void ParkourMapDir(char *out, int outLen) {
    const char *base = GetApplicationDirectory();
    if (!base || !base[0]) base = ".";
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", base);
    /* strip any trailing separators so exactly one joiner follows */
    int len = (int)strlen(tmp);
    while (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) tmp[--len] = 0;
    if (!len) snprintf(tmp, sizeof(tmp), ".");
    snprintf(out, outLen, "%s/maps", tmp);
}

static void MapPath(const char *name, char *out, int outLen) {
    char dir[512];
    ParkourMapDir(dir, sizeof(dir));
    snprintf(out, outLen, "%s/%s.pmap", dir, name);
}

static bool EnsureMapDir(void) {
    char dir[512];
    ParkourMapDir(dir, sizeof(dir));
    if (DirExistsRaw(dir)) return true;
#if defined(OS_LINUX) || defined(PLATFORM_WEB)
    mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#else
    mkdir(dir);
#endif
    if (DirExistsRaw(dir)) return true;
    snprintf(lastError, sizeof(lastError), "cannot create folder %s", dir);
    return false;
}

int ParkourMapList(char names[][PMAP_NAME_LEN], int max) {
    char dir[512];
    ParkourMapDir(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) return 0;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < max) {
        const char *f = ent->d_name;
        size_t n = strlen(f);
        if (n < 6 || strcmp(f + n - 5, ".pmap") != 0) continue;
        snprintf(names[count], PMAP_NAME_LEN, "%.*s", (int)(n - 5), f);
        count++;
    }
    closedir(d);
    /* stable order so picker arrows do not shuffle between rescans */
    for (int i = 1; i < count; i++)
        for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char tmp[PMAP_NAME_LEN];
            memcpy(tmp, names[j - 1], PMAP_NAME_LEN);
            memcpy(names[j - 1], names[j], PMAP_NAME_LEN);
            memcpy(names[j], tmp, PMAP_NAME_LEN);
        }
    return count;
}

bool ParkourMapLoad(const char *name, PMap *out) {
    char path[600];
    MapPath(name, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(lastError, sizeof(lastError), "cannot open %s", path);
        return false;
    }
    char buf[160 * 1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    memset(out, 0, sizeof(*out));
    snprintf(out->name, sizeof(out->name), "%s", name);
    char *line = buf;
    while (line && *line) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        int a, b, c, d2, e, g, h;
        if (sscanf(line, "box %d %d %d %d %d %d %d", &a, &b, &c, &d2, &e, &g, &h) == 7) {
            if (out->boxCount < PMAP_MAX_BOXES) {
                PMapBox *box = &out->boxes[out->boxCount++];
                box->x = a; box->y = b; box->z = c;
                box->hx = d2; box->hy = e; box->hz = g; box->id = h;
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
    lastError[0] = 0;
    return true;
}

bool ParkourMapSave(const PMap *map) {
    if (!map->name[0]) {
        snprintf(lastError, sizeof(lastError), "map name is empty");
        return false;
    }
    if (!EnsureMapDir()) return false;
    char path[600];
    MapPath(map->name, path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(lastError, sizeof(lastError), "cannot write %s", path);
        return false;
    }
    if (map->hasGate)
        fprintf(f, "gate %d %d %d\n", map->gate[0], map->gate[1], map->gate[2]);
    if (map->hasStart)
        fprintf(f, "start %d %d %d\n", map->start[0], map->start[1], map->start[2]);
    if (map->hasFinish)
        fprintf(f, "finish %d %d %d\n", map->finish[0], map->finish[1], map->finish[2]);
    for (int i = 0; i < map->boxCount; i++) {
        const PMapBox *b = &map->boxes[i];
        fprintf(f, "box %d %d %d %d %d %d %d\n",
                b->x, b->y, b->z, b->hx, b->hy, b->hz, b->id);
    }
    bool flushOk = (fflush(f) == 0);
    fclose(f);
    /* verify the file really landed on disk before reporting success */
    struct stat st;
    if (!flushOk || stat(path, &st) != 0 || st.st_size <= 0) {
        snprintf(lastError, sizeof(lastError), "write failed for %s", path);
        return false;
    }
    lastError[0] = 0;
    return true;
}

bool ParkourMapDelete(const char *name) {
    char path[600];
    MapPath(name, path, sizeof(path));
    if (remove(path) != 0) {
        snprintf(lastError, sizeof(lastError), "cannot delete %s", path);
        return false;
    }
    lastError[0] = 0;
    return true;
}

static PMap activeMap;
static bool activeValid = false;
static char activeName[PMAP_NAME_LEN] = "";
static bool testWarp = false;

void ParkourMapSetTestWarp(bool on) { testWarp = on; }
bool ParkourMapTestWarp(void) { return testWarp; }

static bool warpRequested = false;
void ParkourMapRequestWarp(void) { warpRequested = true; }
bool ParkourMapTakeWarpRequest(void) {
    bool was = warpRequested;
    warpRequested = false;
    return was;
}

void ParkourMapSetSession(const PMap *map) {
    if (map) {
        activeMap = *map;
        activeValid = true;
        snprintf(activeName, PMAP_NAME_LEN, "%s", map->name);
    } else {
        activeValid = false;
    }
}

void ParkourMapSetActive(const char *name) {
    activeValid = false;
    activeName[0] = 0;
    testWarp = false;   /* only the editor TEST button arms the warp */
    if (!name || !name[0] || strcmp(name, "default") == 0) return;
    if (ParkourMapLoad(name, &activeMap)) {
        activeValid = true;
        snprintf(activeName, PMAP_NAME_LEN, "%s", name);
    }
}

const char *ParkourMapActiveName(void) { return activeName; }
const PMap *ParkourMapActive(void) { return activeValid ? &activeMap : NULL; }

void ParkourMapAnchor(int *x, int *y, int *z) {
    /* v65.37: the field is 384x384x128 centred on the second pocket
     * zone centre (-1200, 1200), floor top at 118 - anchor = centre - 192 */
    *x = -1200 - 192;
    *y = 118;
    *z = 1200 - 192;
}
