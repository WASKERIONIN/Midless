/*
 * Midless: Cosmic Edition - pocket universe dreamcore ambience (v65.13,
 * rebuilt in v65.15). Palette sampled from liminal-space references:
 * periwinkle upper sky melting into pink, then a peach horizon glow,
 * a pastel cloud collar hugging the island rim and a cloud sea below.
 */
#include "pocketdreams.h"
#include "pocketfx.h"
#include <math.h>

static Texture2D cloudTex;
static Texture2D skyTex;      /* 1 x 128 vertical gradient */
static bool ready;

void PocketDreams_Init(void) {
    /* --- cloud puff: smoothstep falloff, no boosted core. The v65.14
     * alpha*1.8 turned puffs into glowing paper panels. --- */
    Image im = GenImageColor(64, 64, BLANK);
    Color *px = (Color *)im.data;   /* RGBA8888: Color-sized pixels */
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            float dx = (x - 31.5f) / 31.5f;
            float dy = (y - 31.5f) / 15.0f;   /* flattened: clouds are wide */
            float d = sqrtf(dx * dx + dy * dy);
            float a = 1.0f - d;
            if (a < 0.0f) a = 0.0f;
            a = a * a * (3.0f - 2.0f * a);    /* smoothstep: soft everywhere */
            /* lumpy top edge, dreamcore cumulus */
            a *= 0.80f + 0.20f * sinf(dx * 9.0f + dy * 3.0f);
            px[y * 64 + x] = (Color){ 255, 255, 255, (unsigned char)(a * 255.0f) };
        }
    }
    cloudTex = LoadTextureFromImage(im);
    SetTextureFilter(cloudTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(im);

    /* --- the dreamcore sky: periwinkle -> pink -> peach -> cream --- */
    Image sk = GenImageColor(1, 128, BLANK);
    Color *sp = (Color *)sk.data;
    static const unsigned char stops[4][3] = {
        { 150, 156, 208 },   /* 0.00 periwinkle */
        { 214, 178, 204 },   /* 0.42 pink */
        { 246, 204, 190 },   /* 0.72 peach */
        { 250, 228, 206 },   /* 1.00 cream glow */
    };
    static const float stopsAt[4] = { 0.0f, 0.42f, 0.72f, 1.0f };
    for (int y = 0; y < 128; y++) {
        float t = y / 127.0f;
        int s = 0;
        while (s < 2 && t > stopsAt[s + 1]) s++;
        float f = (t - stopsAt[s]) / (stopsAt[s + 1] - stopsAt[s]);
        if (f < 0.0f) f = 0.0f;
        if (f > 1.0f) f = 1.0f;
        f = f * f * (3.0f - 2.0f * f);
        unsigned char r = (unsigned char)(stops[s][0] + (stops[s + 1][0] - stops[s][0]) * f);
        unsigned char g = (unsigned char)(stops[s][1] + (stops[s + 1][1] - stops[s][1]) * f);
        unsigned char b = (unsigned char)(stops[s][2] + (stops[s + 1][2] - stops[s][2]) * f);
        sp[y] = (Color){ r, g, b, 255 };
    }
    skyTex = LoadTextureFromImage(sk);
    SetTextureFilter(skyTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(sk);

    ready = cloudTex.id != 0 && skyTex.id != 0;
}

/* v65.15: the flat pocket sky colour read as a mauve bedsheet. This
 * paints the reference gradient over the clear colour, blended by the
 * pocket factor; the 3D world then draws on top of it. */
void PocketDreams_DrawSky(float pocketFactor) {
    if (!ready || pocketFactor <= 0.01f) return;
    Color tint = WHITE;
    tint.a = (unsigned char)(255.0f * pocketFactor);
    DrawTexturePro(skyTex,
        (Rectangle){ 0, 0, (float)skyTex.width, (float)skyTex.height },
        (Rectangle){ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() },
        (Vector2){ 0, 0 }, 0.0f, tint);
}

void PocketDreams_Draw(Camera camera, float pocketFactor) {
    if (!ready || pocketFactor <= 0.02f) return;
    double t = GetTime();

    static const Color seaTint[3] = {
        { 242, 196, 208, 255 },   /* pink */
        { 196, 172, 208, 255 },   /* lavender */
        { 246, 208, 184, 255 },   /* peach */
    };

    /* v65.15: the rim collar - OUTSIDE the platform (radius 70-95 vs
     * the lawn's 64) and mostly BELOW the turf line, so from above it
     * reads as a cloud ring hugging the island instead of the v65.14
     * streaks lying across the lawn; from eye level its tops peek
     * over the rim and kill the hard cut line. */
    unsigned char collar = (unsigned char)(205.0f * pocketFactor);
    for (int i = 0; i < 40; i++) {
        float ang = i * 0.1571f + (float)t * 0.004f;
        float rad = 82.0f + 13.0f * sinf(i * 2.3f);
        Vector3 pos = {
            POCKETFX_CX + cosf(ang) * rad,
            143.0f + 5.0f * sinf(i * 1.7f),
            POCKETFX_CZ + sinf(ang) * rad,
        };
        Color tint = seaTint[i % 3];
        tint.a = collar;
        DrawBillboard(camera, cloudTex, pos, 34.0f + 12.0f * sinf(i * 3.1f), tint);
    }

    /* the under-sea: pastel cloud floor far below - look over the edge
     * and the island floats in cloud, no void drop */
    unsigned char under = (unsigned char)(175.0f * pocketFactor);
    for (int i = 0; i < 22; i++) {
        float ang = i * 0.2856f + (float)t * 0.002f;
        float rad = 95.0f + 150.0f * fabsf(sinf(i * 4.7f));
        Vector3 pos = {
            POCKETFX_CX + cosf(ang) * rad,
            108.0f + 16.0f * sinf(i * 2.7f),
            POCKETFX_CZ + sinf(ang) * rad,
        };
        Color tint = seaTint[(i + 1) % 3];
        tint.a = under;
        DrawBillboard(camera, cloudTex, pos, 90.0f + 45.0f * sinf(i * 1.9f), tint);
    }

    /* high drifters: soft blobs in the periwinkle */
    unsigned char high = (unsigned char)(110.0f * pocketFactor);
    for (int i = 0; i < 7; i++) {
        float ang = i * 0.9f + (float)t * 0.01f;
        float rad = 80.0f + 60.0f * sinf(i * 4.7f);
        Vector3 pos = {
            POCKETFX_CX + cosf(ang) * rad,
            188.0f + 8.0f * sinf(i * 2.9f + (float)t * 0.05f),
            POCKETFX_CZ + sinf(ang) * rad,
        };
        Color tint = { 240, 214, 224, 255 };
        tint.a = high;
        DrawBillboard(camera, cloudTex, pos, 40.0f + 14.0f * sinf(i * 1.3f), tint);
    }
}
