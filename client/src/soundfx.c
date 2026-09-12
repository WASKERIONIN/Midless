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
    /* v59.8: a gentle hatch - fibrous husk tearing (filtered noise with
     * a slow falling brightness) over a warm bloom that rises then
     * settles. No metallic partials, no sub thump. */
    float huskLp = 0.0f;
    float tonePhase = 0.0f, fifthPhase = 0.0f;
    unsigned int seed = 4451u;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / frames;
        seed = seed * 1664525u + 1013904223u;
        float n = ((int)seed % 20000) / 10000.0f - 1.0f;
        float bright = 0.35f * expf(-t * 3.2f) + 0.06f;
        huskLp += (n - huskLp) * (bright * 0.9f + 0.02f);
        if (huskLp < 1e-15f && huskLp > -1e-15f) huskLp = 0.0f;
        float huskEnv = expf(-t * 4.0f) * (t < 0.02f ? t / 0.02f : 1.0f);
        float husk = huskLp * 1.6f * huskEnv;
        float f = 196.0f + 58.0f * sinf(3.1416f * (t < 0.7f ? t / 0.7f : 1.0f));
        tonePhase += 6.28318f * f / 22050.0f;
        if (tonePhase > 6.28318f) tonePhase -= 6.28318f;
        fifthPhase += 6.28318f * f * 1.4983f / 22050.0f;
        if (fifthPhase > 6.28318f) fifthPhase -= 6.28318f;
        float bloomEnv = (t < 0.10f ? t / 0.10f : 1.0f) * expf(-t * 2.6f);
        float bloom = (sinf(tonePhase) + 0.35f * sinf(fifthPhase)) * bloomEnv * 0.24f;
        float v = husk * 0.42f + bloom;
        if (v > 0.92f) v = 0.92f;
        if (v < -0.92f) v = -0.92f;
        data[i] = (short)(v * 11000.0f);
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
    NOTE_D5 = 587.33f, NOTE_F5 = 698.46f, NOTE_BB3 = 233.08f, NOTE_GS4 = 415.30f, NOTE_E2 = 82.41f,
    NOTE_D2 = 73.42f, NOTE_B4 = 493.88f, NOTE_E5 = 659.26f, NOTE_G5 = 783.99f;

typedef struct MusTrack {
    const char *name;         /* v59.7: station name for the HUD */
    const float *chords;      /* 4 chords x 3 notes (freqs), 0 = rest */
    const float *motif;       /* melody quote, freqs */
    int motifLen;
    float rootBase;           /* bass anchor */
    int dorian;               /* scale flavour */
    /* v59.4: per-track character. All four loops used to share one tempo,
     * one timbre and one filter - a listener could not tell the radio
     * ever changed. Every knob below is per track now. */
    float barSec;             /* seconds per chord bar */
    float bassDiv;            /* bass pulses per bar (0 = slow swell) */
    float bassGain, bassDecay;
    float padGain, padHarm;   /* pad level, 2nd-harmonic amount */
    float leadGain, leadOct;  /* melody level, octave shift */
    float lpCoef;             /* lowpass brightness */
    int   bellBars;           /* toll every N bars (0 = none) */
    float bellFreq;
    int   drone;              /* hold the first chord only */
    /* v59.6: actual instruments - each station plays its own lead voice
     * and pad family instead of one shared timbre */
    int   leadVoice;          /* 0 soft, 1 lute pluck, 2 breath flute,
                               * 3 choir, 4 glass bell */
    int   padVoice;           /* 0 warm pad, 1 organ, 2 airy breath, 3 strings */
} MusTrack;

/* v59.4: a held root+fifth - the abyss does not chord, it breathes */
static const float chordsAbyss[] = {
    NOTE_D3, NOTE_A3, 0,  NOTE_D3, NOTE_A3, 0,
    NOTE_D3, NOTE_A3, 0,  NOTE_D3, NOTE_A3, 0,
};

/* L'homme arme (opening, simplified, D dorian) + a chant answer */
static const float motifHomme[] = {
    NOTE_D4, NOTE_D4, NOTE_D5, NOTE_A4, NOTE_G4, NOTE_A4, NOTE_C5, NOTE_A4,
    NOTE_G4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_D4, 0, NOTE_D4, 0
};
static const float motifChant[] = {
    NOTE_A4, NOTE_C5, NOTE_D5, NOTE_C5, NOTE_A4, NOTE_G4, NOTE_A4, 0,
    NOTE_E4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_E4, NOTE_D4, NOTE_E4, 0
};
/* v59.6: Dies irae - the 13th-century Gregorian plainchant opening
 * (public domain), the most quoted doom motif in music; it leads the
 * Wraith procession now */
static const float motifDies[] = {
    NOTE_F4, NOTE_E4, NOTE_F4, NOTE_D4,  NOTE_E4, NOTE_C4, NOTE_D4, NOTE_D4,
    NOTE_F4, NOTE_E4, NOTE_F4, NOTE_D4,  NOTE_E4, NOTE_C4, NOTE_D4, 0
};
/* v59.7: Veni Veni Emmanuel - the 12th-century Advent plainchant
 * (public domain), sung in catacombs and cold chapels */
static const float motifVeni[] = {
    NOTE_E4, NOTE_E4, NOTE_G4, NOTE_A4,  NOTE_A4, NOTE_G4, NOTE_A4, NOTE_B4,
    NOTE_C5, NOTE_B4, NOTE_A4, NOTE_G4,  NOTE_A4, NOTE_G4, NOTE_E4, 0
};
/* v59.7: Greensleeves - the 16th-century English tune (public domain),
 * here as a fading music-box memory over the meadows */
static const float motifGreen[] = {
    NOTE_A4, NOTE_C5, NOTE_D5, NOTE_E5,  NOTE_F5, NOTE_E5, NOTE_D5, NOTE_B4,
    NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,  NOTE_A4, 0, NOTE_A4, 0
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
/* v59.7: Em - Am - C - G for the catacomb vigil */
static const float chordsVigil[] = {
    NOTE_E3, NOTE_G3, NOTE_B3,  NOTE_A3, NOTE_C4, NOTE_E4,
    NOTE_C4, NOTE_E4, NOTE_G4,  NOTE_G3, NOTE_B3, NOTE_D4,
};
/* v59.7: Am - G - Am - E(half cadence) under Greensleeves */
static const float chordsGhost[] = {
    NOTE_A3, NOTE_C4, NOTE_E4,  NOTE_G3, NOTE_B3, NOTE_D4,
    NOTE_A3, NOTE_C4, NOTE_E4,  NOTE_E3, NOTE_GS4, NOTE_B3,
};

#define MUS_NTRACKS 6

static const MusTrack tracks[MUS_NTRACKS] = {
    /* 0: dorian stride, SOFT LUTE lead, warm pad. L'homme arme (15th c.). */
    { "Wanderer's March", chordsAm,     motifHomme,  16, NOTE_A2, 0, 3.8f, 2.0f, 0.44f, 5.5f, 0.26f, 0.30f, 0.17f, 1.0f, 0.20f, 4, 880.0f, 0, 1, 0 },
    /* 1: one held fifth, breath FLUTE, airy whisper pad. */
    { "The Abyss",        chordsAbyss,  motifChant,  16, NOTE_D2, 1, 7.4f, 0.0f, 0.40f, 0.0f, 0.32f, 0.10f, 0.11f, 0.5f, 0.10f, 2, 220.0f, 1, 2, 2 },
    /* 2: STRING pad, CHOIR lead, Dies irae plainchant (13th c.). */
    { "Wraith Procession", chordsWraith, motifDies,  16, NOTE_D2, 0, 4.6f, 4.0f, 0.42f, 7.5f, 0.24f, 0.45f, 0.19f, 1.0f, 0.15f, 3, 660.0f, 0, 3, 3 },
    /* 3: v59.8 re-voiced dark like the Vigil: warm low pad, the glass
     * bell an octave down and far softer, darker filter */
    { "Frozen Chapel",    chordsChapel, motifChapel, 16, NOTE_E2, 0, 5.8f, 0.0f, 0.32f, 0.0f, 0.30f, 0.20f, 0.13f, 1.0f, 0.14f, 1, 880.0f, 0, 4, 0 },
    /* 4: Veni Emmanuel plainchant (12th c.), organ breath + flute. */
    { "Catacomb Vigil",   chordsVigil,  motifVeni,   16, NOTE_E2, 1, 6.6f, 0.0f, 0.36f, 0.0f, 0.30f, 0.60f, 0.12f, 1.0f, 0.13f, 2, 440.0f, 0, 2, 1 },
    /* 5: v59.8 re-voiced: a slow choir carries Greensleeves over soft
     * strings (the bare music-box sine read as cheap synthesis) */
    { "Ghost of the Green", chordsGhost, motifGreen, 16, NOTE_A2, 1, 4.4f, 0.0f, 0.38f, 0.0f, 0.26f, 0.20f, 0.15f, 1.0f, 0.15f, 0, 880.0f, 0, 3, 3 },
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
    int pick = (int)(Mus_NextRand() % (MUS_NTRACKS - 1u));
    if (pick >= musTrack) pick++;     /* 0..5 minus current */
    return pick;
}

static unsigned int musSample = 0;
static float musLp = 0.0f;
/* per-voice continuous phases survive across callback calls */
static float padPhase[3] = { 0, 0, 0 };
static float padDet[3] = { 0, 0, 0 };
static float padSawLp[3] = { 0, 0, 0 };
static float bassPhase = 0.0f;
static float melPhase = 0.0f;
static float melPhaseB = 0.0f;   /* detuned partner voice (choir/airy) */

/* v59.6: white noise for breathy timbres */
static unsigned int musNSeed = 22221u;
static float Mus_Noise(void) {
    musNSeed = musNSeed * 1664525u + 1013904223u;
    return (float)(musNSeed >> 8) / 8388608.0f - 1.0f;
}

/* v59.6: the lead instruments. `ph` is the base phase accumulated by the
 * sampler; every voice keeps its own partner phases so timbres keep
 * their character across notes and across stream callbacks. */
static float Mus_LeadVoice(int voice, float freq, float ph, float env) {
    static float vibPh = 0.0f, tremPh = 0.0f;
    switch (voice) {
        case 1: {   /* v59.7 soft lute: two DETUNED deep-rolled saws plus
                     * a warm sine fundamental - the single hard-rolled
                     * saw sounded like a toy keyboard */
            static float plkA = 0.0f, plkB = 0.0f;
            melPhaseB += 6.28318f * freq * 1.0045f / MUS_SR;
            if (melPhaseB > 6.28318f) melPhaseB -= 6.28318f;
            float sawA = ph / 3.14159f - 1.0f;
            float sawB = melPhaseB / 3.14159f - 1.0f;
            plkA += (sawA - plkA) * 0.13f;
            plkB += (sawB - plkB) * 0.13f;
            if (plkA < 1e-15f && plkA > -1e-15f) plkA = 0.0f;  /* denormal flush */
            if (plkB < 1e-15f && plkB > -1e-15f) plkB = 0.0f;
            float body = (plkA + plkB) * 0.5f + 0.42f * sinf(ph);
            return body * env;
        }
        case 2: {   /* breath flute: sine + octave, vibrato, a whiff of air */
            vibPh += 6.28318f * 4.7f / MUS_SR;
            if (vibPh > 6.28318f) vibPh -= 6.28318f;
            float wobble = 1.0f + 0.006f * sinf(vibPh);
            float breathe = Mus_Noise() * 0.09f * env;
            return (sinf(ph * wobble) + 0.22f * sinf(ph * 2.0f * wobble)) * env + breathe;
        }
        case 3: {   /* choir: two detuned voices + soft octave, slow swell */
            melPhaseB += 6.28318f * freq * 1.0062f / MUS_SR;
            if (melPhaseB > 6.28318f) melPhaseB -= 6.28318f;
            tremPh += 6.28318f * 0.45f / MUS_SR;
            if (tremPh > 6.28318f) tremPh -= 6.28318f;
            float swell = 0.75f + 0.25f * sinf(tremPh);
            return (sinf(ph) + sinf(melPhaseB) + 0.45f * sinf(ph * 2.0f)) * env * swell / 1.45f;
        }
        case 4: {   /* glass bell: v59.8 - rounded partials, no ear-stab */
            melPhaseB += 6.28318f * freq * 1.76f / MUS_SR;
            if (melPhaseB > 6.28318f) melPhaseB -= 6.28318f;
            return (sinf(ph) + 0.26f * sinf(melPhaseB) + 0.08f * sinf(ph * 4.4f)) * env;
        }
        case 5: {   /* v59.7 music box: pure partials, quick shimmer */
            melPhaseB += 6.28318f * freq * 3.01f / MUS_SR;
            if (melPhaseB > 6.28318f) melPhaseB -= 6.28318f;
            return (sinf(ph) + 0.4f * sinf(melPhaseB)) * env;
        }
        default:    /* soft woodwind-ish default */
            return (sinf(ph) + 0.3f * sinf(ph * 2.0f)) * env;
    }
}

static float Mus_NextSample(void) {
    /* v59.7: curNote/xfadePos live at the top so the hand-off and the
     * manual station skip can reset them */
    static float curNote = 0.0f;
    static int xfadePos = 0;

    const MusTrack *T = &tracks[musTrack];
    float BAR = (float)MUS_SR * T->barSec;               /* per-track tempo */
    /* v59.7: the station changes at every 4-bar pass (~15-30 s) and the
     * new track STARTS FROM ITS TOP: no mid-bar envelope jump (that was
     * the click on every hand-off), the sample counter stays small so
     * the bar math never drifts, and the pick is always a DIFFERENT
     * station */
    /* v59.8: background music - four full 8-bar cycles (2-4 minutes)
     * before the station sweep, new track always from its top */
    unsigned int cycle = (unsigned int)(musSample / (BAR * 32.0f));
    if (cycle > 0 && cycle != musLastCycle) {
        musTrack = Mus_PickDifferent();
        T = &tracks[musTrack];
        BAR = (float)MUS_SR * T->barSec;
        musSample = 0;
        curNote = 0;
        xfadePos = 0;
        musLastCycle = 0;
    } else {
        musLastCycle = cycle;
    }
    /* bar position in double, then truncate - the old chain (sample over
     * a float bar, through a truncated ULL multiply) lost precision the
     * longer the game ran and wobbled every envelope */
    unsigned int total = (unsigned int)((double)musSample / (double)BAR);
    int chordIdx = T->drone ? 0 : (int)(total % 4);
    int barIn2 = (int)(total % 8);
    float tInBar = (float)((double)musSample / (double)BAR - (double)total); /* 0..1 */

    const float *ch = &T->chords[chordIdx * 3];

    /* pad: three detuned voices, slow attack/release envelope */
    float env = tInBar < 0.12f ? (tInBar / 0.12f) : (tInBar > 0.88f ? (1.0f - tInBar) / 0.12f : 1.0f);
    float pad = 0.0f;
    for (int v = 0; v < 3; v++) {
        if (ch[v] <= 0.0f) continue;
        padPhase[v] += 6.28318f * (ch[v] * (1.0f + padDet[v])) / MUS_SR;
        if (padPhase[v] > 6.28318f) padPhase[v] -= 6.28318f;
        float s;
        switch (T->padVoice) {
            case 1:   /* church organ: stacked octaves/fifths, steady */
                s = sinf(padPhase[v]) + 0.60f * sinf(padPhase[v] * 2.0f) +
                    0.30f * sinf(padPhase[v] * 3.001f) + 0.12f * sinf(padPhase[v] * 4.0f);
                break;
            case 2:   /* airy breath: detuned partner + a whisper of air */
                s = sinf(padPhase[v]) + 0.45f * sinf(padPhase[v] * 1.007f) +
                    Mus_Noise() * 0.14f;
                break;
            case 3: { /* strings: two rolled-off saws, slow bow */
                float saw = padPhase[v] / 3.14159f - 1.0f;
                padSawLp[v] += (saw - padSawLp[v]) * 0.16f;
                s = padSawLp[v] + 0.4f * sinf(padPhase[v]);
                break; }
            default:  /* warm analog pad */
                s = sinf(padPhase[v]);
                s += T->padHarm * sinf(padPhase[v] * 2.0f);
                s += 0.18f * sinf(padPhase[v] * 3.002f);
                break;
        }
        pad += s * (v == 0 ? 0.34f : 0.26f);
    }
    pad *= env * T->padGain;

    /* bass: a plucked pulse (marches) or a slow swell (drones) */
    bassPhase += 6.28318f * (T->rootBase * 0.5f) / MUS_SR;
    if (bassPhase > 6.28318f) bassPhase -= 6.28318f;
    float bEnv;
    if (T->bassDiv > 0.0f) {
        float beat = fmodf(tInBar * T->bassDiv, 1.0f);
        bEnv = expf(-beat * T->bassDecay) * 0.5f + 0.10f;
    } else {
        bEnv = 0.30f + 0.30f * sinf(3.1416f * tInBar);
    }
    float bass = sinf(bassPhase) * bEnv * T->bassGain;

    /* melody: chant-like walk; quotes the motif every second 8-bar cycle */
    float mel = 0.0f;
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
    melPhase += 6.28318f * (curNote * T->leadOct) / MUS_SR;
    if (melPhase > 6.28318f) melPhase -= 6.28318f;
    float mEnv;
    if (T->leadVoice == 1)        /* lute: gentle strike, soft long decay */
        mEnv = (expf(-(float)xfadePos / (MUS_SR * 0.65f)) * 0.70f + 0.08f) * T->leadGain * 2.1f;
    else if (T->leadVoice == 4)   /* bell: long icy decay */
        mEnv = (expf(-(float)xfadePos / (MUS_SR * 1.60f)) * 0.80f + 0.08f) * T->leadGain * 2.0f;
    else if (T->leadVoice == 5)   /* music box: quick shimmer decay */
        mEnv = (expf(-(float)xfadePos / (MUS_SR * 0.90f)) * 0.80f + 0.06f) * T->leadGain * 2.0f;
    else
        mEnv = (1.0f - stepT * 0.35f) * T->leadGain;
    mel = Mus_LeadVoice(T->leadVoice, curNote * T->leadOct, melPhase, mEnv);

    /* v59.4: a per-track bell toll - chapel ice, abyss tocsin */
    if (T->bellBars > 0 && total % (unsigned int)T->bellBars == 0 && tInBar < 0.30f) {
        float bt = tInBar / 0.30f;
        mel += sinf(6.28318f * T->bellFreq * (float)((double)musSample / MUS_SR)) * expf(-bt * 5.0f) * 0.06f;
    }

    float mix = pad + bass + mel;
    /* one-pole lowpass, per-track brightness */
    musLp += (mix - musLp) * T->lpCoef;
    if (musLp < 1e-15f && musLp > -1e-15f) musLp = 0.0f;   /* denormal flush */
    float outv = musLp * 1.30f + mix * 0.55f;   /* v59.6: louder make-up */

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
        /* v59.7: a synth must never hiss. Any NaN, infinity or runaway
         * envelope folds to silence, and a soft saturation knee replaces
         * the hard clip (clipped squares read as buzzy interference) */
        if (!(v > -4.0f && v < 4.0f)) v = 0.0f;
        v = tanhf(v * 1.15f) * 0.90f;
        d[i * 2] = (short)(v * 16000.0f);
        d[i * 2 + 1] = (short)(v * 14700.0f);
    }
}

/* v59.7: the HUD station label */
const char *SoundFx_TrackName(void) {
    return tracks[musTrack].name;
}

/* v59.7: N skips to the next station; the new one starts from its top.
 * Called on the main thread; every write is a single word, and the
 * audio thread re-reads the state each sample. */
void SoundFx_NextTrack(void) {
    musTrack = Mus_PickDifferent();
    musSample = 0;
    musLastCycle = 0;
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
