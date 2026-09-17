/*
 * Midless: Cosmic Edition - terrain atlas self-heal (v65.11). See atlasheal.h.
 */
#include "atlasheal.h"
#include "resource.h"
#include <stdio.h>

static void HealTile(Color *px, int tile, int atlasWidth) {
    int x0 = (tile % (atlasWidth / 16)) * 16;
    int y0 = (tile / (atlasWidth / 16)) * 16;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            Color c;
            if (tile == 78) {           /* pocket turf: sunny emerald */
                c = ((x + 2 * y) % 5) ? (Color){ 88, 146, 80, 255 }
                                      : (Color){ 64, 112, 58, 255 };
                if ((x * 3 + y) % 11 == 0) c = (Color){ 188, 170, 104, 255 };
            } else if (tile == 79) {    /* pocket loam: warm brown */
                c = ((x + y) % 4) ? (Color){ 128, 92, 58, 255 }
                                  : (Color){ 100, 70, 44, 255 };
                if ((x * 2 + y * 3) % 13 == 0) c = (Color){ 196, 150, 86, 255 };
            } else if (tile == 81) {    /* v65.28 concrete: cool pale grey, seam lines */
                c = ((x + 2 * y) % 6) ? (Color){ 172, 172, 176, 255 }
                                      : (Color){ 142, 142, 148, 255 };
                if (y == 4 || y == 12) c = (Color){ 128, 128, 134, 255 };
            } else if (tile == 82) {    /* v65.28 concrete base: dark cold grey */
                c = ((x + y) % 5) ? (Color){ 96, 98, 108, 255 }
                                  : (Color){ 74, 76, 86, 255 };
            } else if (tile == 83) {    /* v65.28 path lamp: warm panel in a frame */
                bool edge = x < 2 || y < 2 || x > 13 || y > 13;
                c = edge ? (Color){ 120, 120, 126, 255 }
                         : (Color){ 253, 241, 210, 255 };
            } else {                    /* warp gate: cream core, gold ring, violet rim */
                int dx = x - 8, dy = y - 8;
                int d2 = dx * dx + dy * dy;
                if (d2 < 8) c = (Color){ 255, 248, 224, 255 };
                else if (d2 < 26) c = (Color){ 255, 200, 80, 255 };
                else c = ((x + y) % 2) ? (Color){ 150, 80, 205, 255 }
                                       : (Color){ 90, 40, 140, 255 };
            }
            px[(y0 + y) * atlasWidth + (x0 + x)] = c;
        }
    }
}

void AtlasHeal_Terrain(Texture2D *terrain) {
    if (!terrain || !terrain->id) return;
    Image image = Resource_LoadImage("terrain.png");
    if (!image.data || image.width < 16 || image.height < 16 ||
        image.width % 16 != 0 || image.height % 16 != 0) {
        if (image.data) UnloadImage(image);
        return;
    }
    TraceLog(LOG_INFO, "terrain atlas on disk: %dx%d", image.width, image.height);

    Color *px = (Color *)image.data;   /* RGBA8888 = Color layout */
    int cols = image.width / 16;
    bool missing[6] = { false, false, false, false, false, false };
    bool anyMissing = false;
    for (int t = 78; t <= 83; t++) {   /* v65.28: + the Foundry concrete tiles */
        int x = (t % cols) * 16 + 8;
        int y = (t / cols) * 16 + 8;
        if (x >= image.width || y >= image.height || px[y * image.width + x].a == 0) {
            missing[t - 78] = true;
            anyMissing = true;
        }
    }
    if (!anyMissing) {
        UnloadImage(image);
        return;
    }
    for (int t = 78; t <= 83; t++) {
        if (missing[t - 78]) HealTile(px, t, image.width);
    }
    UpdateTexture(*terrain, image.data);
    TraceLog(LOG_WARNING,
             "terrain.png is missing pocket tiles 78-83: synthesized placeholder "
             "pixels in the GPU atlas. Extract the full release zip for the real art.");
    UnloadImage(image);
}
