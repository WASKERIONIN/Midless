#include "soundfx.h"
#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>

static Sound digSnd;
static Sound placeSnd;
static Sound jumpSnd;
static Sound teleportSnd;
static Sound clickSnd;
static Sound windSnd;
static bool ready;
static float volume = 0.7f;

/* deterministic LCG so every install sounds identical */
static unsigned int RngState = 0xA341316Cu;

static float NextNoise(void) {
    RngState = RngState * 1664525u + 1013904223u;
    return ((RngState >> 16) / 32768.0f) - 1.0f;
}

static Sound MakeSound(int frames, void (*fill)(short *, int)) {
    Wave wave = {0};
    wave.frameCount = (unsigned int)frames;
    wave.sampleRate = 22050;
    wave.sampleSize = 16;
    wave.channels = 1;
    short *data = (short *)MemAlloc((size_t)frames * sizeof(short));
    wave.data = data;
    fill(data, frames);
    Sound s = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return s;
}

/* crunchy dig: filtered noise burst + faint crystal ping */
static void FillDig(short *data, int frames) {
    float lp = 0.0f;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        float env = expf(-t * 9.0f);
        float n = NextNoise();
        lp += (n - lp) * 0.22f;
        float ping = sinf(2.0f * PI * 1850.0f * i / 22050.0f) * expf(-t * 26.0f) * 0.18f;
        float crunch = sinf(2.0f * PI * (70.0f + 60.0f * (1.0f - t)) * i / 22050.0f) * 0.30f;
        float v = (lp * 0.75f + crunch + ping) * env;
        data[i] = (short)(v * 15000.0f);
    }
}

/* glass chime place: stacked sines with slow sparkle */
static void FillPlace(short *data, int frames) {
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        float env = expf(-t * 7.5f);
        float v = sinf(2.0f * PI * 1046.5f * i / 22050.0f) * 0.45f +
                  sinf(2.0f * PI * 1568.0f * i / 22050.0f) * 0.30f *
                      (0.6f + 0.4f * sinf(2.0f * PI * 6.0f * t)) +
                  sinf(2.0f * PI * 2093.0f * i / 22050.0f) * 0.22f * expf(-t * 12.0f) +
                  sinf(2.0f * PI * 523.3f * i / 22050.0f) * 0.16f;
        data[i] = (short)(v * env * 11500.0f);
    }
}

/* soft jump whoosh: quick filtered noise swell */
static void FillJump(short *data, int frames) {
    float lp = 0.0f;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        float env = sinf(t * PI);
        float n = NextNoise();
        lp += (n - lp) * 0.10f;
        float sweep = sinf(2.0f * PI * (220.0f + 320.0f * t) * i / 22050.0f) * 0.18f;
        data[i] = (short)((lp * 0.5f + sweep) * env * 7000.0f);
    }
}

/* teleport: rising shimmer sweep + sparkle tail */
static void FillTeleport(short *data, int frames) {
    float phase = 0.0f;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        float freq = 300.0f + 1100.0f * t * t;
        phase += 2.0f * PI * freq / 22050.0f;
        float env = sinf(t * PI) * (t < 0.75f ? 1.0f : expf(-(t - 0.75f) * 14.0f));
        float shimmer = sinf(phase * 2.0f) * 0.22f * (0.5f + 0.5f * sinf(2.0f * PI * 9.0f * t));
        float v = (sinf(phase) * 0.4f + shimmer + NextNoise() * 0.05f) * env;
        data[i] = (short)(v * 13000.0f);
    }
}

/* UI click: tiny rounded blip */
static void FillClick(short *data, int frames) {
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        float env = expf(-t * 16.0f);
        float v = sinf(2.0f * PI * 880.0f * i / 22050.0f) * 0.5f +
                  sinf(2.0f * PI * 1760.0f * i / 22050.0f) * 0.15f;
        data[i] = (short)(v * env * 9000.0f);
    }
}

/* ambient void wind: seamless 4s band-passed brown noise loop */
static void FillWind(short *data, int frames) {
    float lp1 = 0.0f, lp2 = 0.0f;
    /* one wrap of the LCG state so the loop seam stays quiet-ish; the strong
     * low-pass makes any residual step inaudible at low volume */
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        float n = NextNoise();
        lp1 += (n - lp1) * 0.035f;
        lp2 += (lp1 - lp2) * 0.020f;
        float gust = 0.65f + 0.35f * sinf(2.0f * PI * 2.0f * t + 1.7f) * sinf(2.0f * PI * 0.5f * t);
        float v = lp2 * 5.2f * gust;
        if (v > 0.95f) v = 0.95f;
        if (v < -0.95f) v = -0.95f;
        data[i] = (short)(v * 6500.0f);
    }
}

void SoundFx_Init(void) {
    InitAudioDevice();
    SetMasterVolume(volume);
    digSnd = MakeSound(3200, FillDig);
    placeSnd = MakeSound(3600, FillPlace);
    jumpSnd = MakeSound(1400, FillJump);
    teleportSnd = MakeSound(8800, FillTeleport);
    clickSnd = MakeSound(900, FillClick);
    windSnd = MakeSound(22050 * 4, FillWind);
    ready = true;
    if (ready && IsAudioDeviceReady()) PlaySound(windSnd);
}

void SoundFx_Shutdown(void) {
    if (!ready) return;
    UnloadSound(digSnd);
    UnloadSound(placeSnd);
    UnloadSound(jumpSnd);
    UnloadSound(teleportSnd);
    UnloadSound(clickSnd);
    UnloadSound(windSnd);
    CloseAudioDevice();
    ready = false;
}

void SoundFx_Update(void) {
    /* keep the ambient loop alive */
    if (ready && IsAudioDeviceReady() && !IsSoundPlaying(windSnd)) PlaySound(windSnd);
}

void SoundFx_PlayDig(void) { if (ready) PlaySound(digSnd); }
void SoundFx_PlayPlace(void) { if (ready) PlaySound(placeSnd); }
void SoundFx_PlayJump(void) { if (ready) PlaySound(jumpSnd); }
void SoundFx_PlayTeleport(void) { if (ready) PlaySound(teleportSnd); }
void SoundFx_PlayClick(void) { if (ready) PlaySound(clickSnd); }

void SoundFx_SetVolume(float volume01) {
    volume = Clamp(volume01, 0.0f, 1.0f);
    if (IsAudioDeviceReady()) SetMasterVolume(volume);
}

float SoundFx_GetVolume(void) { return volume; }
