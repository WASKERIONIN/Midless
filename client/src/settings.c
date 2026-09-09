#include "settings.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

GameSettings gameSettings = {
    .width = 1280,
    .height = 720,
    .fullscreen = false,
    .maxFpsChoice = 0,
    .drawDistance = 8,
    .volume = 70,
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
            if (value < 2) value = 2;
            if (value > 16) value = 16;
            gameSettings.drawDistance = value;
        } else if (sscanf(line, "volume=%d", &value) == 1) {
            if (value < 0) value = 0;
            if (value > 100) value = 100;
            gameSettings.volume = value;
        }
        line = end ? end + 1 : NULL;
    }
    UnloadFileText(text);
}

void Settings_Save(void) {
    BuildPath();
    char body[256];
    snprintf(body, sizeof(body),
             "width=%d\nheight=%d\nfullscreen=%d\nmaxfps=%d\ndrawdistance=%d\nvolume=%d\n",
             gameSettings.width, gameSettings.height, gameSettings.fullscreen ? 1 : 0,
             gameSettings.maxFpsChoice, gameSettings.drawDistance, gameSettings.volume);
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

void Settings_ApplyWindow(void) {
    if (gameSettings.width < 640) gameSettings.width = 1280;
    if (gameSettings.height < 480) gameSettings.height = 720;
    SetWindowSize(gameSettings.width, gameSettings.height);
    if (gameSettings.fullscreen && !IsWindowFullscreen()) ToggleFullscreen();
    if (!gameSettings.fullscreen && IsWindowFullscreen()) ToggleFullscreen();
    Settings_ApplyMaxFps();
}

void Settings_ToggleFullscreen(void) {
    ToggleFullscreen();
    gameSettings.fullscreen = IsWindowFullscreen();
    if (!gameSettings.fullscreen) SetWindowSize(gameSettings.width, gameSettings.height);
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
