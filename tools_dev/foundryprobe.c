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
        { "plaza floor",      -1230, 118, 1220, 81 },
        { "plaza base",       -1230, 114, 1220, 82 },
        { "plaza air",        -1230, 122, 1220, 0 },
        { "centre gate",      -1240, 118, 1208, 80 },
        { "path lamp",        -1237, 118, 1211, 83 },
        { "rim north",        -1232, 119, 1180, 81 },
        { "flow S1 top",      -1206, 119, 1208, 81 },
        { "flow S1 tower",    -1206, 115, 1208, 82 },
        { "rest pad",         -1178, 121, 1208, 81 },
        { "wall-run face",    -1165, 122, 1204, 81 },
        { "wall top lamp",    -1172, 127, 1204, 83 },
        { "exit ledge",       -1153, 126, 1206, 81 },
        { "chimney a",        -1149, 125, 1207, 81 },
        { "chimney b",        -1146, 125, 1207, 81 },
        { "back shelf",       -1147, 133, 1211, 81 },
        { "precision P1",     -1140, 133, 1212, 81 },
        { "precision P3",     -1126, 133, 1212, 81 },
        { "finish plateau",   -1116, 133, 1212, 81 },
        { "finish rim",       -1116, 134, 1208, 81 },
        { "beacon",           -1116, 135, 1212, 83 },
        { "shortcut base",    -1168, 124, 1210, 81 },
        { "monolith M1",      -1262, 130, 1188, 81 },
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
        for (int x = -1272; x < -1108; x += 1)
            for (int z = 1176; z < 1240; z += 1) {
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
