/* v65.28 foundry probe - evaluates the FROZEN worldgen material field at
 * the second pocket's course coordinates. Dev tool, not shipped:
 *   probe_build-style link with modprobe_stubs.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "scripting/luaengine.h"
#include "world/worldgen.h"

int main(void) {
    Lua_Init();
    LuaBindings_Init();
    if (!Lua_Run()) { printf("FOUNDRYPROBE: LUA RUN FAILED\n"); return 1; }
    if (worldgen.material < 0) { printf("FOUNDRYPROBE: no material field\n"); return 1; }
    printf("FOUNDRYPROBE: mod OK, maxY=%d bounded=%d\n", worldgen.maxY, worldgen.bounded);

    /* name, x, y, z, expected */
    static const struct { const char *n; int x, y, z, e; } probes[] = {
        { "court floor",      -1266, 118, 1206, 81 },
        { "court below",      -1268, 114, 1204, 82 },
        { "court air",        -1268, 122, 1204, 0 },
        { "centre gate",      -1272, 118, 1200, 80 },
        { "leg1 island",      -1252, 118, 1200, 81 },
        { "leg1 pit air",     -1242, 118, 1200, 0 },
        { "leg1 north wall",  -1230, 125, 1206, 81 },
        { "entry nub",        -1246, 121, 1205, 81 },
        { "crest lamp",       -1252, 132, 1206, 83 },
        { "divider crest",    -1170, 127, 1200, 81 },
        { "left outer wall",  -1170, 125, 1215, 81 },
        { "right chim stub",  -1176, 125, 1190, 81 },
        { "leg2 outer west",  -1156, 127, 1230, 81 },
        { "leg2 inner west",  -1152, 127, 1230, 81 },
        { "leg2 slot air",    -1150, 120, 1230, 0 },
        { "leg3 north wall",  -1130, 127, 1256, 81 },
        { "leg3 island",      -1136, 118, 1250, 81 },
        { "finish chimney",   -1122, 126, 1250, 81 },
        { "finish top",       -1114, 135, 1250, 81 },
        { "beacon",           -1114, 137, 1250, 83 },
        { "meadow turf (ctl)", 1204, 154, -1196, 78 },
    };
    int bad = 0;
    for (unsigned i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        WGEval ctx;
        Vector3 p = { probes[i].x, 0, probes[i].z };
        Worldgen_EvalInit(&ctx, p, p);
        Worldgen_EvalY(&ctx, probes[i].y);
        float id = worldgen.bounded && (probes[i].y < worldgen.minY || probes[i].y > worldgen.maxY)
                   ? -1 : Worldgen_Eval(&ctx, worldgen.material);
        int got = id >= 0 ? (int)id : -1;   /* -1 = clipped by bounded maxY */
        const char *verdict = got == probes[i].e ? "ok" : "MISMATCH";
        if (got != probes[i].e) bad++;
        printf("FOUNDRYPROBE: %-18s (%d,%d,%d) = %d (expect %d) %s\n",
               probes[i].n, probes[i].x, probes[i].y, probes[i].z, got, probes[i].e, verdict);
    }
    printf("FOUNDRYPROBE: fieldCount=%d\n", worldgen.fieldCount);
    /* ---- region scan: anything that is NOT course material in the
     * Foundry footprint is a leak (flora, mushrooms, decor) ---- */
    {
        int counts[256] = { 0 };
        for (int x = -1284; x < -1104; x += 1)
            for (int z = 1188; z < 1260; z += 1) {
                WGEval ctx;
                Vector3 p = { x, 0, z };
                Worldgen_EvalInit(&ctx, p, p);
                for (int y = 119; y <= 140; y++) {
                    Worldgen_EvalY(&ctx, y);
                    float id = Worldgen_Eval(&ctx, worldgen.material);
                    int iid = (int)id;
                    if (iid > 0 && iid != 80 && iid != 81 && iid != 82 && iid != 83)
                        counts[iid & 255]++;
                }
            }
        printf("FOUNDRYPROBE: REGION SCAN (leaks above the floor, ids != 80/81/82/83):\n");
        for (int i = 0; i < 256; i++)
            if (counts[i]) printf("FOUNDRYPROBE:   leak id %d x %d cells\n", i, counts[i]);
    }
    printf("FOUNDRYPROBE: %s (%d mismatches)\n", bad ? "FAIL" : "PASS", bad);
    return bad ? 1 : 0;
}
