/*
 * Midless: Cosmic Edition - pocket universe dreamcore ambience (v65.13).
 * Palette sampled from dreamcore references: pink-lavender cloud sea,
 * peach horizon glow, periwinkle high sky.
 */
#include "pocketdreams.h"
#include "pocketfx.h"
#include <math.h>

static Texture2D cloudTex;
static bool ready;

void PocketDreams_Init(void) {
    Image im = GenImageColor(64, 64, BLANK);
    Color *px = (Color *)im.data;   /* RGBA8888: Color-sized pixels */
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            float dx = (x - 31.5f) / 31.5f;
            float dy = (y - 31.5f) / 14.0f;   /* flattened: clouds are wide */
            float d = sqrtf(dx * dx + dy * dy);
            float a = 1.0f - d;
            if (a < 0.0f) a = 0.0f;
            /* v65.14: denser core - the v65.13 falloff left the clouds
             * as faint smudges on a bright sky */
            a = powf(a, 1.1f) * 1.8f;
            if (a > 1.0f) a = 1.0f;
            /* lumpy top edge, dreamcore cumulus */
            a *= 0.75f + 0.25f * sinf(dx * 9.0f + dy * 3.0f);
            px[y * 64 + x] = (Color){ 255, 255, 255, (unsigned char)(a * 255.0f) };
        }
    }
    cloudTex = LoadTextureFromImage(im);
    SetTextureFilter(cloudTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(im);
    ready = cloudTex.id != 0;
}

void PocketDreams_Draw(Camera camera, float pocketFactor) {
    if (!ready || pocketFactor <= 0.02f) return;
    double t = GetTime();
    unsigned char wall = (unsigned char)(245.0f * pocketFactor);
    unsigned char under = (unsigned char)(225.0f * pocketFactor);
    unsigned char high = (unsigned char)(150.0f * pocketFactor);

    static const Color seaTint[3] = {
        { 248, 208, 218, 255 },   /* pink */
        { 208, 184, 218, 255 },   /* lavender */
        { 252, 218, 194, 255 },   /* peach */
    };

    /* v65.14: the rim wall. The meadow is HALF=64 blocks wide; the
     * v65.13 ring sat at 108+ - a visible strip of void between lawn
     * edge and clouds. This wall straddles the rim itself (62 +- 8),
     * big and near-opaque, so the platform dissolves into cloud. */
    for (int i = 0; i < 34; i++) {
        float ang = i * 0.1848f + (float)t * 0.004f;
        float rad = 62.0f + 8.0f * sinf(i * 2.3f);
        Vector3 pos = {
            POCKETFX_CX + cosf(ang) * rad,
            152.0f + 4.0f * sinf(i * 1.7f),
            POCKETFX_CZ + sinf(ang) * rad,
        };
        Color tint = seaTint[i % 3];
        tint.a = wall;
        DrawBillboard(camera, cloudTex, pos, 46.0f + 16.0f * sinf(i * 3.1f), tint);
    }

    /* v65.14: the under-sea - clouds BELOW the platform. Look over
     * the edge and there is no void drop, just a pastel sea below:
     * the island floats in cloud, the dreamcore staple. */
    for (int i = 0; i < 26; i++) {
        float ang = i * 0.2417f + (float)t * 0.002f;
        float rad = 46.0f + 62.0f * fabsf(sinf(i * 4.7f));
        Vector3 pos = {
            POCKETFX_CX + cosf(ang) * rad,
            122.0f + 16.0f * sinf(i * 2.7f),
            POCKETFX_CZ + sinf(ang) * rad,
        };
        Color tint = seaTint[(i + 1) % 3];
        tint.a = under;
        DrawBillboard(camera, cloudTex, pos, 62.0f + 30.0f * sinf(i * 1.9f), tint);
    }

    /* high drifters: the liminal far sky */
    for (int i = 0; i < 7; i++) {
        float ang = i * 0.9f + (float)t * 0.01f;
        float rad = 60.0f + 40.0f * sinf(i * 4.7f);
        Vector3 pos = {
            POCKETFX_CX + cosf(ang) * rad,
            176.0f + 6.0f * sinf(i * 2.9f + (float)t * 0.05f),
            POCKETFX_CZ + sinf(ang) * rad,
        };
        Color tint = { 242, 216, 226, 255 };
        tint.a = high;
        DrawBillboard(camera, cloudTex, pos, 30.0f + 10.0f * sinf(i * 1.3f), tint);
    }
}
