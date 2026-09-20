/* v65.41 host probe: parse the bundled dragon.vox and verify the data
 * the client renderer relies on (dims, voxel bounds, palette).
 * Build: gcc -std=c99 -Iclient/src tools_dev/voxprobe.c client/src/voxparse.c -o build/probe/voxprobe
 * Run from the repo root. */
#include "voxparse.h"
#include <stdio.h>

int main(void) {
    VoxData v;
    if (!VoxParseFile("models/vox/dragon.vox", &v)) {
        printf("VOXPROBE: FAIL parse dragon.vox\n");
        return 1;
    }
    int minz = 1 << 30, maxz = -1, bad = 0;
    for (int i = 0; i < v.count; i++) {
        if (v.vox[i][0] >= v.sx || v.vox[i][1] >= v.sy || v.vox[i][2] >= v.sz) bad++;
        if (v.vox[i][2] < minz) minz = v.vox[i][2];
        if (v.vox[i][2] > maxz) maxz = v.vox[i][2];
    }
    printf("VOXPROBE: dragon.vox size=%dx%dx%d voxels=%d palette=%s h(z)=%d..%d\n",
           v.sx, v.sy, v.sz, v.count, v.hasPalette ? "yes" : "NO", minz, maxz);
    VoxFree(&v);
    if (bad || !v.hasPalette) { printf("VOXPROBE: FAIL (%d out-of-bounds)\n", bad); return 1; }
    printf("VOXPROBE: PASS\n");
    return 0;
}
