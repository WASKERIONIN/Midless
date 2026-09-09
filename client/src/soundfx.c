#include "soundfx.h"
#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>

static Sound digSnd;
static Sound placeSnd;
static bool ready;
static float volume = 0.7f;

static Sound MakeNoise(int frames, int freq, float decay) {
    Wave wave = {0};
    wave.frameCount = (unsigned int)frames;
    wave.sampleRate = 22050;
    wave.sampleSize = 16;
    wave.channels = 1;
    short *data = (short *)MemAlloc((size_t)frames * sizeof(short));
    wave.data = data;
    unsigned int rng = 0xA341316Cu;
    for (int i = 0; i < frames; i++) {
        rng = rng * 1664525u + 1013904223u;
        float t = (float)i / (float)frames;
        float env = expf(-decay * t);
        float tone = sinf(2.0f * PI * (float)freq * (float)i / 22050.0f);
        float n = ((rng >> 16) / 32768.0f) - 1.0f;
        data[i] = (short)((tone * 0.35f + n * 0.65f) * env * 18000.0f);
    }
    Sound s = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return s;
}

void SoundFx_Init(void) {
    InitAudioDevice();
    SetMasterVolume(volume);
    digSnd = MakeNoise(2800, 90, 6.0f);
    placeSnd = MakeNoise(2200, 140, 8.0f);
    ready = true;
}

void SoundFx_Shutdown(void) {
    if (!ready) return;
    UnloadSound(digSnd);
    UnloadSound(placeSnd);
    CloseAudioDevice();
    ready = false;
}

void SoundFx_PlayDig(void) {
    if (ready) PlaySound(digSnd);
}

void SoundFx_PlayPlace(void) {
    if (ready) PlaySound(placeSnd);
}

void SoundFx_SetVolume(float volume01) {
    volume = Clamp(volume01, 0.0f, 1.0f);
    if (IsAudioDeviceReady()) SetMasterVolume(volume);
}

float SoundFx_GetVolume(void) { return volume; }
