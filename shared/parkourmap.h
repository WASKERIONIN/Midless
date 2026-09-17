#ifndef MIDLESS_PARKOURMAP_H
#define MIDLESS_PARKOURMAP_H
/* v65.32: user-authored parkour maps. The editor (client) writes .pmap
 * files into maps/ next to the executable; the server overlays the chosen
 * map onto the second pocket zone at chunk-generation time, replacing the
 * built-in course. Local coordinates: x/z 0..191, y 0..63 - the overlay
 * anchors them at (P2 anchor - 96, floor top, P2 anchor - 96). */
#include <stdbool.h>

#define PMAP_MAX_BOXES 4096
#define PMAP_MAX_MAPS  64
#define PMAP_NAME_LEN  48

typedef struct PMapBox {
    int x, y, z;          /* box centre (local) */
    int hx, hy, hz;       /* half extents in cells */
    int id;               /* block id */
} PMapBox;

typedef struct PMap {
    char name[PMAP_NAME_LEN];
    PMapBox boxes[PMAP_MAX_BOXES];
    int boxCount;
    int gate[3], start[3], finish[3];   /* local marker cells */
    bool hasGate, hasStart, hasFinish;
} PMap;

void ParkourMapDir(char *out, int outLen);
int  ParkourMapList(char names[][PMAP_NAME_LEN], int max);
bool ParkourMapLoad(const char *name, PMap *out);
bool ParkourMapSave(const PMap *map);
bool ParkourMapDelete(const char *name);

/* session-active map (single process: editor/host sets it, the server
 * world generator reads it). Empty name = built-in course. */
void        ParkourMapSetActive(const char *name);
/* v65.34: editor TEST drops the player straight onto the map start */
void        ParkourMapSetTestWarp(bool on);
bool        ParkourMapTestWarp(void);
const char *ParkourMapLastError(void);
void        ParkourMapSetSession(const PMap *map);   /* in-memory (dev probes) */
const char *ParkourMapActiveName(void);
const PMap *ParkourMapActive(void);   /* NULL when the built-in course runs */

/* world-space anchor of local (0,0,0) */
void ParkourMapAnchor(int *x, int *y, int *z);
#endif
