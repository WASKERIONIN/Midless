#include "soundfx.h"
#include "raylib.h"
#include "hunter.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>

static Sound digSnd;
static Sound placeSnd;
static Sound jumpSnd;
static Sound teleportSnd;
static Sound clickSnd;
static Sound windSnd;
static Sound droneSnd;   /* dungeon synth pedal drone */
static Sound bellSnd;    /* distant temple bell */
static Sound hunterHitSnd;
static Sound hunterDieSnd;
static Sound hurtSnd;
static Sound webShootSnd;
static Sound boomSnd;
static Sound webAttachSnd;
static bool ready;
static float volume = 0.7f;
static double nextBellIn = 30.0;

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

/* v45 hunter hit: metallic tick - bright sine snap with noise edge */
static void FillHunterHit(short *data, int frames) {
    for (int i = 0; i < frames; i++) {
        float t = (float)i / 22050.0f;
        float env = expf(-t * 55.0f);
        float v = sinf(2.0f * PI * 1150.0f * t) * 0.6f
                + sinf(2.0f * PI * 1720.0f * t) * 0.25f
                + NextNoise() * 0.2f * expf(-t * 90.0f);
        data[i] = (short)(v * env * 11500.0f);
    }
}

/* v45 hunter death: descending sweep + crumble */
static void FillHunterDie(short *data, int frames) {
    float phase = 0.0f;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / 22050.0f;
        float env = expf(-t * 6.5f);
        float f = 880.0f * expf(-t * 4.2f) + 90.0f;
        phase += 2.0f * PI * f / 22050.0f;
        float v = sinf(phase) * 0.55f + NextNoise() * 0.3f * expf(-t * 9.0f);
        data[i] = (short)(v * env * 11500.0f);
    }
}

/* v45 player hurt: low dull thud */
static void FillHurt(short *data, int frames) {
    float phase = 0.0f;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / 22050.0f;
        float env = expf(-t * 26.0f);
        float f = 120.0f * expf(-t * 18.0f) + 46.0f;
        phase += 2.0f * PI * f / 22050.0f;
        float v = sinf(phase) * 0.85f + NextNoise() * 0.12f;
        data[i] = (short)(v * env * 12000.0f);
    }
}

/* v51 explosion: deep boom with debris rumble */
static void FillExplosion(short *data, int frames) {
    float phase = 0.0f, phase2 = 0.0f;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / 22050.0f;
        float env = expf(-t * 5.5f);
        float f = 150.0f * expf(-t * 7.0f) + 38.0f;
        phase += 2.0f * PI * f / 22050.0f;
        phase2 += 2.0f * PI * (f * 1.47f) / 22050.0f;
        float v = sinf(phase) * 0.8f + sinf(phase2) * 0.25f
                + NextNoise() * 0.5f * expf(-t * 10.0f);
        data[i] = (short)(v * env * 12500.0f);
    }
}

/* v46 web shoot: airy zip up */
static void FillWebShoot(short *data, int frames) {
    float phase = 0.0f;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / 22050.0f;
        float env = expf(-t * 22.0f);
        float f = 300.0f + 2400.0f * (1.0f - expf(-t * 30.0f));
        phase += 2.0f * PI * f / 22050.0f;
        float v = sinf(phase) * 0.5f + NextNoise() * 0.10f;
        data[i] = (short)(v * env * 9000.0f);
    }
}

/* v46 web attach: crisp double tick */
static void FillWebAttach(short *data, int frames) {
    for (int i = 0; i < frames; i++) {
        float t = (float)i / 22050.0f;
        float env = expf(-t * 70.0f);
        float tick = (i > 260 && i < 300) ? 0.5f : 0.0f;
        float v = sinf(2.0f * PI * 1900.0f * t) * 0.45f * env
                + NextNoise() * 0.15f * expf(-t * 120.0f)
                + sinf(2.0f * PI * 2600.0f * (t - 0.012f)) * tick * expf(-(t - 0.012f) * 60.0f);
        data[i] = (short)(v * 10500.0f);
    }
}

/* dungeon synth drone: detuned low sines with a breathing slow LFO,
 * seamless 8-second loop */
static void FillDrone(short *data, int frames) {
    const float base = 55.0f; /* A1 */
    for (int i = 0; i < frames; i++) {
        float t = (float)i / 22050.0f;
        float lfo = 0.72f + 0.28f * sinf(2.0f * PI * t / 8.0f);
        float v = 0.0f;
        v += sinf(2.0f * PI * base * t) * 0.30f;
        v += sinf(2.0f * PI * base * 1.005f * t + 1.3f) * 0.24f;  /* detune beat */
        v += sinf(2.0f * PI * base * 1.5f * t + 0.4f) * 0.14f;    /* fifth */
        v += sinf(2.0f * PI * base * 2.0f * t + 2.2f) * 0.09f;    /* octave */
        v += sinf(2.0f * PI * base * 2.997f * t + 0.9f) * 0.05f;  /* shimmer */
        data[i] = (short)(v * lfo * 5200.0f);
    }
}

/* distant temple bell: inharmonic partials, long fade */
static void FillBell(short *data, int frames) {
    for (int i = 0; i < frames; i++) {
        float t = (float)i / 22050.0f;
        float env = expf(-t * 1.35f);
        float strike = expf(-t * 22.0f);
        float v = 0.0f;
        v += sinf(2.0f * PI * 220.0f * t) * 0.34f;
        v += sinf(2.0f * PI * 220.0f * 2.74f * t) * 0.20f * expf(-t * 2.2f);
        v += sinf(2.0f * PI * 220.0f * 5.42f * t) * 0.12f * expf(-t * 3.6f);
        v += sinf(2.0f * PI * 220.0f * 8.13f * t) * 0.07f * expf(-t * 5.0f);
        v += sinf(2.0f * PI * 110.0f * t) * 0.18f;
        v += NextNoise() * strike * 0.16f;
        data[i] = (short)(v * env * 10500.0f);
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
    droneSnd = MakeSound(22050 * 8, FillDrone);
    bellSnd = MakeSound(22050 * 5, FillBell);
    hunterHitSnd = MakeSound(1600, FillHunterHit);
    hunterDieSnd = MakeSound(5100, FillHunterDie);
    hurtSnd = MakeSound(1700, FillHurt);
    webShootSnd = MakeSound(1800, FillWebShoot);
    boomSnd = MakeSound(22050 * 2, FillExplosion);
    webAttachSnd = MakeSound(900, FillWebAttach);
    ready = true;
    if (ready && IsAudioDeviceReady()) {
        PlaySound(windSnd);
        PlaySound(droneSnd);
    }
}

void SoundFx_Shutdown(void) {
    if (!ready) return;
    UnloadSound(digSnd);
    UnloadSound(placeSnd);
    UnloadSound(jumpSnd);
    UnloadSound(teleportSnd);
    UnloadSound(clickSnd);
    UnloadSound(windSnd);
    UnloadSound(droneSnd);
    UnloadSound(bellSnd);
    UnloadSound(hunterHitSnd);
    UnloadSound(hunterDieSnd);
    UnloadSound(hurtSnd);
    UnloadSound(webShootSnd);
    UnloadSound(webAttachSnd);
    UnloadSound(boomSnd);
    CloseAudioDevice();
    ready = false;
}

void SoundFx_Update(void) {
    if (!ready || !IsAudioDeviceReady()) return;
    /* keep the ambient loops alive */
    if (!IsSoundPlaying(windSnd)) {
        PlaySound(windSnd);
        SetSoundVolume(windSnd, 0.55f);
    }
    if (!IsSoundPlaying(droneSnd)) {
        PlaySound(droneSnd);
        SetSoundVolume(droneSnd, 0.6f);
    }
    /* v47.1: the dungeon drone swells when the void tide rises */
    SetSoundVolume(droneSnd, 0.55f + 0.4f * Hunter_GetSurgeLevel());
    /* dungeon synth moment: a far-away bell tolls now and then */
    nextBellIn -= GetFrameTime();
    if (nextBellIn <= 0.0f) {
        nextBellIn = 34.0f + (float)(GetRandomValue(0, 3200)) / 100.0f;
        SetSoundPitch(bellSnd, 0.78f + (float)GetRandomValue(0, 44) / 100.0f);
        PlaySound(bellSnd);
        SetSoundVolume(bellSnd, 0.5f);
    }
}

void SoundFx_PlayDig(void) { if (ready) PlaySound(digSnd); }
void SoundFx_PlayPlace(void) { if (ready) PlaySound(placeSnd); }
void SoundFx_PlayJump(void) { if (ready) PlaySound(jumpSnd); }
void SoundFx_PlayTeleport(void) { if (ready) PlaySound(teleportSnd); }
void SoundFx_PlayClick(void) { if (ready) PlaySound(clickSnd); }

void SoundFx_PlayHunterHit(void) { if (ready) PlaySound(hunterHitSnd); }
void SoundFx_PlayHunterDie(void) { if (ready) PlaySound(hunterDieSnd); }
void SoundFx_PlayPlayerHurt(void) { if (ready) PlaySound(hurtSnd); }
void SoundFx_PlayWebShoot(void) { if (ready) PlaySound(webShootSnd); }
void SoundFx_PlayWebAttach(void) { if (ready) PlaySound(webAttachSnd); }
void SoundFx_PlayExplosion(void) { if (ready) PlaySound(boomSnd); }

void SoundFx_SetVolume(float volume01) {
    volume = Clamp(volume01, 0.0f, 1.0f);
    if (IsAudioDeviceReady()) SetMasterVolume(volume);
}

float SoundFx_GetVolume(void) { return volume; }
