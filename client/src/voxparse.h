#ifndef MIDLESS_VOXPARSE_H
#define MIDLESS_VOXPARSE_H
/* v65.40: MagicaVoxel .vox parser - pure C, no raylib, host-probeable.
 * Reads the classic single-model layout: VOX header + MAIN { SIZE, XYZI,
 * RGBA? }. Models by ephtracy (see models/CREDITS.txt). */
#include <stdint.h>
#include <stdbool.h>

typedef struct VoxData {
    int sx, sy, sz;           /* dims; z is the HEIGHT axis (MagicaVoxel) */
    int count;                /* voxel count */
    uint8_t (*vox)[4];        /* count entries: x, y, z, palette index */
    uint8_t palette[256][4];  /* RGBA; palette[i-1] = colour of index i */
    bool hasPalette;
} VoxData;

bool VoxParseFile(const char *path, VoxData *out);
bool VoxParseMemory(const unsigned char *data, int len, VoxData *out);
void VoxFree(VoxData *v);

#endif
