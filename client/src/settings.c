#include "settings.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

GameSettings gameSettings = {
    .width = 1920,     /* v56: launch at 1080p / monitor size by default */
    .height = 1080,
    .fullscreen = true,    /* borderless at the monitor's own resolution */
    .maxFpsChoice = 0,
    .drawDistance = 8,
    .volume = 70,
    .vsync = 1,
    .language = 0,
    .music = 1,
    .resver = 2,
};

static const int kResolutions[][2] = {
    {1280, 720},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
};

static char settingsPath[512];

static void BuildPath(void) {
    snprintf(settingsPath, sizeof(settingsPath), "%ssettings.ini", GetApplicationDirectory());
}

void Settings_Load(void) {
    BuildPath();
    char *text = LoadFileText(settingsPath);
    if (!text) return;
    /* v56: an existing pre-v56 file has no resver key - reset so the
     * one-time migration below fires; first runs keep the default */
    gameSettings.resver = 0;
    char *line = text;
    while (line && *line) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        int value = 0;
        if (sscanf(line, "width=%d", &value) == 1 && value >= 640) gameSettings.width = value;
        else if (sscanf(line, "height=%d", &value) == 1 && value >= 480) gameSettings.height = value;
        else if (sscanf(line, "fullscreen=%d", &value) == 1) gameSettings.fullscreen = value != 0;
        else if (sscanf(line, "maxfps=%d", &value) == 1) gameSettings.maxFpsChoice = value;
        else if (sscanf(line, "drawdistance=%d", &value) == 1) {
            (void)value; /* v47.1: draw distance is fixed at 20 */
        } else if (sscanf(line, "volume=%d", &value) == 1) {
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            gameSettings.volume = value;
        }
        else if (sscanf(line, "vsync=%d", &value) == 1) gameSettings.vsync = value != 0;
        else if (sscanf(line, "language=%d", &value) == 1) gameSettings.language = value;
        else if (sscanf(line, "music=%d", &value) == 1) gameSettings.music = value != 0;
        else if (sscanf(line, "resver=%d", &value) == 1) gameSettings.resver = value;
        line = end ? end + 1 : NULL;
    }
    UnloadFileText(text);
    /* v56: one-time migration - everyone gets the fullscreen-at-monitor
     * launch once; after that their own choices are respected */
    if (gameSettings.resver < 2) {
        gameSettings.resver = 2;
        gameSettings.fullscreen = true;
        gameSettings.width = 1920;
        gameSettings.height = 1080;
        Settings_Save();
    }
}

void Settings_Save(void) {
    BuildPath();
    char body[256];
    snprintf(body, sizeof(body),
             "width=%d\nheight=%d\nfullscreen=%d\nmaxfps=%d\ndrawdistance=%d\nvolume=%d\n"
             "vsync=%d\nlanguage=%d\nmusic=%d\nresver=%d\n",
             gameSettings.width, gameSettings.height, gameSettings.fullscreen ? 1 : 0,
             gameSettings.maxFpsChoice, gameSettings.drawDistance, gameSettings.volume,
             gameSettings.vsync ? 1 : 0, gameSettings.language,
             gameSettings.music ? 1 : 0, gameSettings.resver);
    SaveFileText(settingsPath, body);
}

void Settings_ApplyMaxFps(void) {
    if (gameSettings.maxFpsChoice == 1) SetTargetFPS(120);
    else if (gameSettings.maxFpsChoice == 2) SetTargetFPS(0);
    else {
        gameSettings.maxFpsChoice = 0;
        SetTargetFPS(60);
    }
}

/* v43.3: borderless fullscreen. raylib's ToggleFullscreen() hands the window
 * size to the monitor as a video mode, which changes the user's desktop
 * resolution; a borderless window at the monitor's current size never does. */
static bool borderlessActive = false;

static void Settings_ApplyFullscreenState(void) {
    if (gameSettings.fullscreen && !borderlessActive) {
        int monitor = GetCurrentMonitor();
        SetWindowState(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST);
        SetWindowPosition(0, 0);
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        borderlessActive = true;
    } else if (!gameSettings.fullscreen && borderlessActive) {
        ClearWindowState(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST);
        SetWindowSize(gameSettings.width, gameSettings.height);
        /* raylib 4.5 has no CenterWindow(); center manually */
        int monitor = GetCurrentMonitor();
        SetWindowPosition((GetMonitorWidth(monitor) - gameSettings.width) / 2,
                          (GetMonitorHeight(monitor) - gameSettings.height) / 2);
        borderlessActive = false;
    }
}

void Settings_ApplyWindow(void) {
    if (gameSettings.width < 640) gameSettings.width = 1280;
    if (gameSettings.height < 480) gameSettings.height = 720;
    if (!borderlessActive) SetWindowSize(gameSettings.width, gameSettings.height);
    Settings_ApplyFullscreenState();
    Settings_ApplyMaxFps();
}

void Settings_ToggleFullscreen(void) {
    if (!gameSettings.fullscreen) {
        /* remember the windowed size we are leaving */
        if (!borderlessActive) {
            gameSettings.width = GetScreenWidth();
            gameSettings.height = GetScreenHeight();
        }
    }
    gameSettings.fullscreen = !gameSettings.fullscreen;
    Settings_ApplyFullscreenState();
    Settings_Save();
}

void Settings_CycleResolution(void) {
    int count = (int)(sizeof(kResolutions) / sizeof(kResolutions[0]));
    int index = 0;
    for (int i = 0; i < count; i++) {
        if (kResolutions[i][0] == gameSettings.width && kResolutions[i][1] == gameSettings.height) {
            index = (i + 1) % count;
            break;
        }
        if (i == count - 1) index = 0;
    }
    gameSettings.width = kResolutions[index][0];
    gameSettings.height = kResolutions[index][1];
    if (!IsWindowFullscreen()) SetWindowSize(gameSettings.width, gameSettings.height);
    Settings_Save();
}

const char *Settings_ResolutionLabel(void) {
    return TextFormat("%dx%d", gameSettings.width, gameSettings.height);
}
