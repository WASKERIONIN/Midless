#ifndef MIDLESS_CLIENT_SETTINGS_H
#define MIDLESS_CLIENT_SETTINGS_H

#include <stdbool.h>

typedef struct GameSettings {
    int width;
    int height;
    bool fullscreen;
    int maxFpsChoice;
    int drawDistance;
    int volume;
    int vsync;    /* v56: FLAG_VSYNC_HINT, applied at window creation */
    int resver;   /* v56: settings-layout version for one-time migrations */
} GameSettings;

extern GameSettings gameSettings;

void Settings_Load(void);
void Settings_Save(void);
void Settings_ApplyWindow(void);
void Settings_ToggleFullscreen(void);
void Settings_CycleResolution(void);
const char *Settings_ResolutionLabel(void);
void Settings_ApplyMaxFps(void);

#endif
