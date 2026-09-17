#include "usermusic.h"
#include "soundfx.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

/* v65.35: user playlist for the parkour zone. Everything lives on the
 * main thread (Load/Update/Unload are raylib music-stream calls); the
 * only cross-thread surface is the duck flag, a single bool the audio
 * callback reads. */

#define UM_MAX_TRACKS 64
#define UM_EXT_COUNT  6

static const char *UM_EXTS[UM_EXT_COUNT] = { ".mp3", ".ogg", ".wav", ".flac", ".xm", ".mod" };

static char umPaths[UM_MAX_TRACKS][512];
static char umNames[UM_MAX_TRACKS][64];
static int umCount = 0;
static int umIndex = 0;

static Music umMusic;
static bool umLoaded = false;
static bool umActive = false;     /* inside the parkour zone with tracks */
static bool umPlaying = false;    /* stream actually running */
static bool umDuck = false;       /* silence the built-in synth station */
static bool umInZone = false;     /* inside the parkour zone (diag label) */
static char umLabel[80] = "";

void UserMusic_Dir(char *out, int outLen);
void UserMusic_Dir(char *out, int outLen) {
    const char *base = GetApplicationDirectory();
    if (!base || !base[0]) base = ".";
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", base);
    int len = (int)strlen(tmp);
    while (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) tmp[--len] = 0;
    if (!len) snprintf(tmp, sizeof(tmp), ".");
    snprintf(out, outLen, "%s/music", tmp);
}

static bool HasMusicExt(const char *f) {
    /* v65.37: case-insensitive - "Track.MP3" must count too */
    size_t n = strlen(f);
    for (int e = 0; e < UM_EXT_COUNT; e++) {
        size_t el = strlen(UM_EXTS[e]);
        if (n <= el) continue;
        const char *tail = f + n - el;
        size_t k = 0;
        for (; k < el; k++) {
            char a = tail[k], b = UM_EXTS[e][k];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (k == el) return true;
    }
    return false;
}

static void Scan(void) {
    char dir[512];
    UserMusic_Dir(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) { umCount = 0; return; }
    umCount = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && umCount < UM_MAX_TRACKS) {
        if (!HasMusicExt(ent->d_name)) continue;
        snprintf(umPaths[umCount], sizeof(umPaths[umCount]), "%s/%s", dir, ent->d_name);
        /* display name = file name without the extension */
        snprintf(umNames[umCount], sizeof(umNames[umCount]), "%s", ent->d_name);
        char *dot = strrchr(umNames[umCount], '.');
        if (dot) *dot = 0;
        umCount++;
    }
    closedir(d);
    /* stable alphabetical order */
    for (int i = 1; i < umCount; i++)
        for (int j = i; j > 0 && strcmp(umNames[j - 1], umNames[j]) > 0; j--) {
            char tp[512], tn[64];
            memcpy(tp, umPaths[j - 1], sizeof(tp)); memcpy(umPaths[j - 1], umPaths[j], sizeof(tp)); memcpy(umPaths[j], tp, sizeof(tp));
            memcpy(tn, umNames[j - 1], sizeof(tn)); memcpy(umNames[j - 1], umNames[j], sizeof(tn)); memcpy(umNames[j], tn, sizeof(tn));
        }
}

void UserMusic_Init(void) {
    char dir[512];
    UserMusic_Dir(dir, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) != 0) {
#if defined(OS_LINUX) || defined(PLATFORM_WEB)
        mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#else
        mkdir(dir);
#endif
        /* a readme so the folder explains itself */
        char rp[600];
        snprintf(rp, sizeof(rp), "%s/PUT YOUR MP3 FILES HERE.txt", dir);
        FILE *f = fopen(rp, "wb");
        if (f) {
            fputs("Drop .mp3 / .ogg / .wav / .flac files into this folder.\r\n"
                  "They become the PARKOUR MODE playlist: the music plays\r\n"
                  "while you are inside the parkour instance.\r\n"
                  "N - next track, B - previous track.\r\n"
                  "An empty folder keeps the built-in Foundry station.\r\n", f);
            fclose(f);
        }
    }
    Scan();
}

static void LoadTrack(int i) {
    if (umLoaded) {
        StopMusicStream(umMusic);
        UnloadMusicStream(umMusic);
        umLoaded = false;
        umPlaying = false;
    }
    if (umCount <= 0 || i < 0 || i >= umCount) { umDuck = false; umLabel[0] = 0; return; }
    umIndex = i;
    umMusic = LoadMusicStream(umPaths[i]);
    if (umMusic.frameCount == 0) {   /* unsupported/corrupt file - skip it */
        umDuck = false;
        snprintf(umLabel, sizeof(umLabel), "%s (cannot play)", umNames[i]);
        return;
    }
    umLoaded = true;
    SetMusicVolume(umMusic, SoundFx_GetVolume() * 0.9f);
    snprintf(umLabel, sizeof(umLabel), "%s", umNames[i]);
    umDuck = true;
    if (SoundFx_MusicOn()) {
        PlayMusicStream(umMusic);
        umPlaying = true;
    }
}

void UserMusic_EnterParkour(void) {
    umInZone = true;
    Scan();   /* picks up files dropped since launch */
    if (umCount == 0) { umActive = false; umDuck = false; return; }
    umActive = true;
    LoadTrack(umIndex % umCount);
}

void UserMusic_LeaveParkour(void) {
    umInZone = false;
    umActive = false;
    umDuck = false;
    if (umLoaded) {
        StopMusicStream(umMusic);
        UnloadMusicStream(umMusic);
        umLoaded = false;
    }
    umPlaying = false;
}

void UserMusic_Update(void) {
    if (!umActive || !umLoaded) return;
    if (!SoundFx_MusicOn()) {
        if (umPlaying) { StopMusicStream(umMusic); umPlaying = false; }
        return;
    }
    if (!umPlaying) {
        PlayMusicStream(umMusic);
        umPlaying = true;
        return;
    }
    if (IsMusicStreamPlaying(umMusic)) {
        UpdateMusicStream(umMusic);
        SetMusicVolume(umMusic, SoundFx_GetVolume() * 0.9f);
    } else {
        /* track finished - roll to the next one */
        LoadTrack((umIndex + 1) % umCount);
    }
}

void UserMusic_Next(void) {
    /* v65.37: N doubles as a live retry - drop a file in while inside
     * the zone and press N, the playlist picks it up without a restart */
    if (!umActive || umCount == 0) {
        if (!umInZone) return;
        Scan();
        if (umCount == 0) return;
        umActive = true;
        LoadTrack(0);
        return;
    }
    LoadTrack((umIndex + 1) % umCount);
}

void UserMusic_Prev(void) {
    if (!umActive || umCount == 0) return;
    LoadTrack((umIndex - 1 + umCount) % umCount);
}

bool UserMusic_Playing(void) { return umActive && umCount > 0 && umDuck; }
bool UserMusic_InParkour(void) { return umInZone; }
bool UserMusic_Ducking(void) { return umDuck; }
const char *UserMusic_TrackName(void) { return umLabel; }
int UserMusic_Count(void) { return umCount; }
