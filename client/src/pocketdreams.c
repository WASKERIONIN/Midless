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
            a = a * a * (1.35f - 0.35f * fabsf(dx));   /* soft shoulders */
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
    unsigned char sea = (unsigned char)(190.0f * pocketFactor);
    unsigned char high = (unsigned char)(120.0f * pocketFactor);

    /* the cloud sea: a ring around the meadow that swallows the rim */
    static const Color seaTint[3] = {
        { 246, 206, 216, 255 },   /* pink */
        { 206, 182, 216, 255 },   /* lavender */
        { 250, 216, 192, 255 },   /* peach */
    };
    for (int i = 0; i < 26; i++) {
        float ang = i * 0.2417f + (float)t * 0.004f;
        float rad = 108.0f + 26.0f * sinf(i * 2.3f);
        Vector3 pos = {
            POCKETFX_CX + cosf(ang) * rad,
            151.0f + 3.0f * sinf(i * 1.7f),
            POCKETFX_CZ + sinf(ang) * rad,
        };
        Color tint = seaTint[i % 3];
        tint.a = sea;
        DrawBillboard(camera, cloudTex, pos, 46.0f + 18.0f * sinf(i * 3.1f), tint);
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
        Color tint = { 240, 214, 224, 255 };
        tint.a = high;
        DrawBillboard(camera, cloudTex, pos, 30.0f + 10.0f * sinf(i * 1.3f), tint);
    }
}
