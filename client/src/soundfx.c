#include "soundfx.h"
#include "raylib.h"
#include "settings.h"
#include "hunter.h"
#include "raymath.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
static Sound cocoonSnd;   /* v59.2: the special hatch bloom */
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

/* v59.2: cocoon opening - a soft sub bloom, a shimmer of inharmonic bell
 * partials and the dry rustle of the shell splitting. Replaces the harsh
 * hit sound that used to carry the moment. */
static void FillCocoonOpen(short *data, int frames) {
    float subPhase = 0.0f;
    float part[4];
    float det[4] = { 1.0f, 2.718f, 5.131f, 8.312f };
    float partPhase[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; i++) part[i] = 0.0f;
    unsigned int seed = 20260912u;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        /* sub: 170 -> 46 Hz, blooms then fades */
        float f = 46.0f + (170.0f - 46.0f) * expf(-t * 5.0f);
        subPhase += 6.28318f * f / 22050.0f;
        if (subPhase > 6.28318f) subPhase -= 6.28318f;
        float sub = sinf(subPhase) * expf(-t * 3.2f) * 0.55f * (t < 0.04f ? t / 0.04f : 1.0f);
        /* shimmer: partials fade in over 0.15 s, ring out */
        float shimmer = 0.0f;
        for (int p = 0; p < 4; p++) {
            float amp = (0.30f / (p + 1)) * expf(-t * (2.1f + 0.7f * p)) *
                        (t > 0.12f + 0.05f * p ? 1.0f : t / (0.12f + 0.05f * p));
            partPhase[p] += 6.28318f * (520.0f * det[p] * (1.0f + 0.004f * sinf(t * 9.0f + p))) / 22050.0f;
            if (partPhase[p] > 6.28318f) partPhase[p] -= 6.28318f;
            shimmer += sinf(partPhase[p]) * amp;
        }
        /* shell rustle: decaying noise pops in the first 0.35 s */
        seed = seed * 1664525u + 1013904223u;
        float n = ((int)seed % 20000) / 10000.0f - 1.0f;
        float rustle = n * expf(-t * 9.0f) * 0.30f * (0.6f + 0.4f * sinf(t * 60.0f));
        float v = sub + shimmer * 0.16f + rustle;
        if (v > 0.95f) v = 0.95f;
        if (v < -0.95f) v = -0.95f;
        data[i] = (short)(v * 9000.0f);
    }
}

/* ---------------- v59: dungeon-synth radio ------------------------------
 * A tiny procedural synth streamed through an audio callback. Two loops
 * built on the classic dungeon-synth changes (Aeolian shuttle Am-G-F-G and
 * the Dm-C drone), a chant-like stepwise melody that always returns to its
 * anchor, and quotes of the medieval tune L'homme arme (public domain,
 * 15th c.). Everything is computed in the callback - no asset files. */
static AudioStream musicStream;
static bool musicReady = false;
static bool musicEnabled = true;
static volatile float musicVol = 0.42f;

#define MUS_SR 22050
/* two bars of 4/4 at BPM 52 per buffer step; sequencer is sample-based */
static const float NOTE_A2 = 110.0f, NOTE_C3 = 130.81f, NOTE_D3 = 146.83f,
    NOTE_E3 = 164.81f, NOTE_F3 = 174.61f, NOTE_G3 = 196.0f, NOTE_A3 = 220.0f,
    NOTE_B3 = 246.94f, NOTE_C4 = 261.63f, NOTE_D4 = 293.66f, NOTE_E4 = 329.63f,
    NOTE_F4 = 349.23f, NOTE_G4 = 392.0f, NOTE_A4 = 440.0f, NOTE_C5 = 523.25f,
    NOTE_D5 = 587.33f, NOTE_BB3 = 233.08f, NOTE_GS4 = 415.30f, NOTE_E2 = 82.41f,
    NOTE_D2 = 73.42f, NOTE_B4 = 493.88f, NOTE_E5 = 659.26f, NOTE_G5 = 783.99f;

typedef struct MusTrack {
    const float *chords;      /* 4 chords x 3 notes (freqs), 0 = rest */
    const float *motif;       /* melody quote, freqs */
    int motifLen;
    float rootBase;           /* bass anchor */
    int dorian;               /* scale flavour */
} MusTrack;

/* L'homme arme (opening, simplified, D dorian) + a chant answer */
static const float motifHomme[] = {
    NOTE_D4, NOTE_D4, NOTE_D5, NOTE_A4, NOTE_G4, NOTE_A4, NOTE_C5, NOTE_A4,
    NOTE_G4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_D4, 0, NOTE_D4, 0
};
static const float motifChant[] = {
    NOTE_A4, NOTE_C5, NOTE_D5, NOTE_C5, NOTE_A4, NOTE_G4, NOTE_A4, 0,
    NOTE_E4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_E4, NOTE_D4, NOTE_E4, 0
};
/* v59.2: "Wraith march" - a grim stepwise line over Dm - Bb - C - Dm */
static const float motifWraith[] = {
    NOTE_D4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4, NOTE_F4, NOTE_D4,
    NOTE_C4, NOTE_D4, NOTE_F4, NOTE_E4, NOTE_D4, 0, NOTE_C4, 0
};
/* v59.2: "Frozen chapel" - the Andalusian lament (Am-G-F-E) answered by
 * a thin choir third */
static const float motifChapel[] = {
    NOTE_E5, NOTE_D5, NOTE_C5, NOTE_B4, NOTE_A4, NOTE_GS4, NOTE_A4, 0,
    NOTE_C5, NOTE_B4, NOTE_A4, NOTE_G4, NOTE_A4, 0, NOTE_E4, 0
};

/* Track 1: Am - G - F - G (Aeolian shuttle) */
static const float chordsAm[] = {
    NOTE_A3, NOTE_C4, NOTE_E4,  NOTE_G3, NOTE_B3, NOTE_D4,
    NOTE_F3, NOTE_A3, NOTE_C4,  NOTE_G3, NOTE_B3, NOTE_D4,
};
/* Track 2: Dm - C drone shuttle */
static const float chordsDm[] = {
    NOTE_D4, NOTE_F4, NOTE_A4,  NOTE_D4, NOTE_F4, NOTE_A4,
    NOTE_C4, NOTE_E4, NOTE_G4,  NOTE_C4, NOTE_E4, NOTE_G4,
};
/* Track 3: Dm - Bb - C - Dm wraith march */
static const float chordsWraith[] = {
    NOTE_D4, NOTE_F4, NOTE_A4,  NOTE_BB3, NOTE_D4, NOTE_F4,
    NOTE_C4, NOTE_E4, NOTE_G4,  NOTE_D4, NOTE_F4, NOTE_A4,
};
/* Track 4: Am - G - F - E Andalusian lament */
static const float chordsChapel[] = {
    NOTE_A3, NOTE_C4, NOTE_E4,  NOTE_G3, NOTE_B3, NOTE_D4,
    NOTE_F3, NOTE_A3, NOTE_C4,  NOTE_E3, NOTE_GS4, NOTE_B3,
};

static const MusTrack tracks[4] = {
    { chordsAm,     motifHomme,  16, NOTE_A2, 0 },
    { chordsDm,     motifChant,  16, NOTE_D2, 1 },
    { chordsWraith, motifWraith, 16, NOTE_D2, 0 },
    { chordsChapel, motifChapel, 16, NOTE_E2, 0 },
};

/* v59.3: the radio changes tracks - a fresh pick at every start (seeded
 * from the clock, NOT raylib's unseeded rand) and a hand-off to a
 * DIFFERENT track at every 8-bar boundary (~37 s) while playing */
static unsigned int musLastCycle = 0xFFFFFFFFu;
static unsigned int musSeed = 0u;
static unsigned int Mus_NextRand(void) {
    musSeed = musSeed * 1664525u + 1013904223u;
    return musSeed >> 8;
}
static int musTrack = 0;

static int Mus_PickDifferent(void) {
    int pick = (int)(Mus_NextRand() % 3u);
    if (pick >= musTrack) pick++;     /* 0..3 minus current */
    return pick;
}

static unsigned int musSample = 0;
static float musLp = 0.0f;
/* per-voice continuous phases survive across callback calls */
static float padPhase[3] = { 0, 0, 0 };
static float padDet[3] = { 0, 0, 0 };
static float bassPhase = 0.0f;
static float melPhase = 0.0f;

static float Mus_NextSample(void) {
    const MusTrack *T = &tracks[musTrack];
    const float BAR = (float)MUS_SR * 4.6f;              /* one chord, ~4.6 s */
    unsigned int total = (unsigned int)(musSample / BAR);
    int chordIdx = (int)(total % 4);
    int barIn2 = (int)(total % 8);
    /* v59.3: 8-bar boundary -> 70% chance to hand off, always elsewhere */
    unsigned int cycle = (unsigned int)(musSample / (BAR * 8.0f));
    if (cycle != musLastCycle) {
        musLastCycle = cycle;
        if (cycle > 0 && (Mus_NextRand() % 100u) < 70u)
            musTrack = Mus_PickDifferent();
    }
    float tInBar = (float)((double)musSample - (double)((unsigned long long)total * (unsigned long long)BAR)) / BAR; /* 0..1 */

    const float *ch = &T->chords[chordIdx * 3];

    /* pad: three detuned voices, slow attack/release envelope */
    float env = tInBar < 0.12f ? (tInBar / 0.12f) : (tInBar > 0.88f ? (1.0f - tInBar) / 0.12f : 1.0f);
    float pad = 0.0f;
    for (int v = 0; v < 3; v++) {
        if (ch[v] <= 0.0f) continue;
        padPhase[v] += 6.28318f * (ch[v] * (1.0f + padDet[v])) / MUS_SR;
        if (padPhase[v] > 6.28318f) padPhase[v] -= 6.28318f;
        float s = sinf(padPhase[v]);
        s += 0.45f * sinf(padPhase[v] * 2.0f);
        s += 0.22f * sinf(padPhase[v] * 3.002f);
        pad += s * (v == 0 ? 0.34f : 0.26f);
    }
    pad *= env * 0.30f;

    /* bass: root an octave down, soft pulse each half bar */
    bassPhase += 6.28318f * (T->rootBase * 0.5f) / MUS_SR;
    if (bassPhase > 6.28318f) bassPhase -= 6.28318f;
    float beat = fmodf(tInBar * 2.0f, 1.0f);
    float bEnv = expf(-beat * 5.5f) * 0.5f + 0.10f;
    float bass = sinf(bassPhase) * bEnv * 0.42f;

    /* melody: chant-like walk; quotes the motif every second 8-bar cycle */
    float mel = 0.0f;
    static float curNote = 0.0f;
    static int xfadePos = 0;
    int step = (int)(tInBar * 8.0f);                 /* 8 steps per bar */
    float stepT = tInBar * 8.0f - (float)step;
    float want;
    if (barIn2 >= 4) {
        want = T->motif[(step + chordIdx * 4) % T->motifLen];
    } else {
        /* anchored stepwise walk around the chord tones */
        static const float dorian[8] = { 293.66f, 329.63f, 349.23f, 392.0f, 440.0f, 523.25f, 587.33f, 659.26f };
        static const float aeolian[8] = { 220.0f, 246.94f, 261.63f, 329.63f, 349.23f, 392.0f, 440.0f, 523.25f };
        const float *sc = T->dorian ? dorian : aeolian;
        unsigned int h = (total * 31u + (unsigned)step * 17u);
        int deg = (int)(h % 8u);
        want = sc[deg];
        if (step % 4 == 0) want = ch[2] > 0 ? ch[2] : want;   /* return to anchor */
    }
    if (want != curNote) {
        /* tiny legato slide - a breath, not a portamento */
        curNote = want;
        xfadePos = 0;
    }
    xfadePos++;
    melPhase += 6.28318f * curNote / MUS_SR;
    if (melPhase > 6.28318f) melPhase -= 6.28318f;
    float mEnv = (1.0f - stepT * 0.35f) * (step % 2 == 0 ? 0.20f : 0.16f);
    mel = (sinf(melPhase) + 0.3f * sinf(melPhase * 2.0f)) * mEnv;

    /* soft bell shimmer on the first step of every 4th bar */
    if (step == 0 && chordIdx == 0 && tInBar < 0.25f) {
        float bt = tInBar / 0.25f;
        mel += sinf(6.28318f * NOTE_A4 * 2.0f * (float)((double)musSample / MUS_SR)) * expf(-bt * 6.0f) * 0.05f;
    }

    float mix = pad + bass + mel;
    /* one-pole lowpass: deep dungeon haze */
    musLp += (mix - musLp) * 0.16f;
    float outv = musLp * 1.25f + mix * 0.4f;

    musSample++;
    return outv * musicVol;
}

static void MusicCallback(void *bufferData, unsigned int frames) {
    short *d = (short *)bufferData;
    if (!musicEnabled) {
        memset(d, 0, (size_t)frames * 2 * sizeof(short));
        return;
    }
    for (unsigned int i = 0; i < frames; i++) {
        float v = Mus_NextSample();
        if (v > 0.95f) v = 0.95f;
        if (v < -0.95f) v = -0.95f;
        d[i * 2] = (short)(v * 9000.0f);
        d[i * 2 + 1] = (short)(v * 8200.0f);
    }
}

void SoundFx_SetMusicEnabled(bool on) {
    musicEnabled = on;
    if (musicReady) {
        if (on) PlayAudioStream(musicStream);
        else StopAudioStream(musicStream);
    }
}

void SoundFx_Init(void) {
    InitAudioDevice();
    SetMasterVolume(volume);
    SetAudioStreamBufferSizeDefault(2048);
    musicStream = LoadAudioStream(MUS_SR, 16, 2);
    if (musicStream.buffer != NULL) {
        SetAudioStreamCallback(musicStream, MusicCallback);
        PlayAudioStream(musicStream);   /* callback gate by musicEnabled */
        musicReady = true;
        musicEnabled = gameSettings.music != 0;
        if (!musicEnabled) StopAudioStream(musicStream);
        /* v59.3: every launch starts on a random side, seeded by the
         * clock - raylib's rand may be unseeded this early, which is why
         * the same track played at every start */
        musSeed = (unsigned int)time(NULL) ^ (unsigned int)clock();
        Mus_NextRand(); Mus_NextRand();
        musTrack = (int)(Mus_NextRand() % 4u);
    }
    digSnd = MakeSound(3200, FillDig);
    placeSnd = MakeSound(3600, FillPlace);
    jumpSnd = MakeSound(1400, FillJump);
    teleportSnd = MakeSound(8800, FillTeleport);
    cocoonSnd = MakeSound(48510, FillCocoonOpen);   /* v59.2 */
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
void SoundFx_PlayCocoonOpen(void) { if (ready) PlaySound(cocoonSnd); }   /* v59.2 */
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
