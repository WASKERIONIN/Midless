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
        { "plaza floor",      -1190, 118, 1210, 81 },
        { "plaza base",       -1190, 114, 1210, 82 },
        { "plaza air",        -1190, 122, 1210, 0 },
        { "centre gate",      -1200, 118, 1200, 80 },
        { "path lamp",        -1197, 118, 1203, 83 },
        { "platform A",       -1180, 120, 1214, 81 },
        { "platform D",       -1161, 125, 1212, 81 },
        { "traverse wall",    -1145, 130, 1209, 81 },
        { "ledge L",          -1142, 133, 1213, 81 },
        { "chimney a",        -1147, 139, 1218, 81 },
        { "chimney crown lamp",-1150, 145, 1218, 83 },
        { "hop P",            -1153, 146, 1224, 81 },
        { "finish plateau",   -1172, 148, 1220, 81 },
        { "beacon",           -1172, 150, 1220, 83 },
        { "monolith M2",      -1191, 124, 1219, 81 },
        { "monolith M1",      -1222, 134, 1207, 81 },
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
    printf("FOUNDRYPROBE: %s (%d mismatches)\n", bad ? "FAIL" : "PASS", bad);
    return bad ? 1 : 0;
}
