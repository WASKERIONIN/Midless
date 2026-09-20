/* v65.40 host probe: parse every bundled .vox model and verify the
 * data the client renderer relies on (dims, voxel count, palette).
 * Build: gcc -std=c99 -Iclient/src tools_dev/voxprobe.c client/src/voxparse.c -o build/probe/voxprobe
 * Run from the repo root. */
#include "voxparse.h"
#include <stdio.h>
#include <string.h>

static const char *MODELS[] = {
    "chr_knight.vox", "chr_man.vox", "chr_gumi.vox", "chr_sword.vox",
    "chr_cat.vox", "chr_fox.vox", "T-Rex.vox", "deer.vox",
    "teapot.vox", "monu9.vox", "monu0.vox", "dragon.vox",
};

int main(void) {
    int fail = 0;
    for (unsigned k = 0; k < sizeof(MODELS) / sizeof(MODELS[0]); k++) {
        char path[256];
        snprintf(path, sizeof(path), "models/vox/%s", MODELS[k]);
        VoxData v;
        if (!VoxParseFile(path, &v)) {
            printf("VOXPROBE: FAIL parse %s\n", MODELS[k]);
            fail++;
            continue;
        }
        int minx = 1 << 30, maxx = -1, minz = 1 << 30, maxz = -1;
        for (int i = 0; i < v.count; i++) {
            if (v.vox[i][0] < minx) minx = v.vox[i][0];
            if (v.vox[i][0] > maxx) maxx = v.vox[i][0];
            if (v.vox[i][2] < minz) minz = v.vox[i][2];
            if (v.vox[i][2] > maxz) maxz = v.vox[i][2];
            if (v.vox[i][0] >= v.sx || v.vox[i][1] >= v.sy ||
                v.vox[i][2] >= v.sz) {
                printf("VOXPROBE: FAIL %s voxel out of bounds\n", MODELS[k]);
                fail++;
                break;
            }
        }
        if (!v.hasPalette) { printf("VOXPROBE: FAIL %s has no RGBA palette\n", MODELS[k]); fail++; }
        printf("VOXPROBE: %s size=%dx%dx%d voxels=%d palette=%s h(z)=%d..%d\n",
               MODELS[k], v.sx, v.sy, v.sz, v.count, v.hasPalette ? "yes" : "NO", minz, maxz);
        VoxFree(&v);
    }
    if (fail) { printf("VOXPROBE: FAIL (%d)\n", fail); return 1; }
    printf("VOXPROBE: PASS (%u models)\n", (unsigned)(sizeof(MODELS) / sizeof(MODELS[0])));
    return 0;
}
