/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_SCREEN_H
#define MIDLESS_CLIENT_SCREEN_H

typedef enum Screen {
    SCREEN_GAME,
    SCREEN_PAUSE,
    SCREEN_LOADING,
    SCREEN_JOINING,
    SCREEN_LOGIN,
    SCREEN_OPTIONS,
    SCREEN_EDITOR   /* v65.32: parkour map editor workbench */
} Screen;

extern Screen currentScreen;
extern bool screenCursorEnabled;

/* v49.1: laser upgrade menu (opened with B at a warp core) */
void Screens_UpgradeMenuToggle(void);
bool Screens_UpgradeMenuIsOpen(void);
void Screens_InventoryToggle(void);    /* v53: the satchel */
bool Screens_InventoryIsOpen(void);
bool Screens_HotbarConsumeClick(void);  /* v58: LMB on a quick slot does not mine */
extern bool screenShowDebug;
/* v65.3: F4 - the flora work view (ground patches tinted by chunk load) */
extern bool screenShowWorkView;
void Screens_HostPanelToggle(void);   /* v65.7: F6 host info panel */
bool Screens_HostPanelIsOpen(void);

void Screen_Init(Texture2D terrain, bool *exit);
void Screen_Shutdown(void);
void Screen_Switch(Screen screen);
void Screen_BeginSingleplayer(void);

void Screen_Draw(void);

void Screen_DrawGame(void);
void Screen_DrawPause(void);
void Screen_DrawOptions(void);
void Screen_DrawLoading(void);
void Screen_DrawJoining(void);
void Screen_DrawLogin(void);

#endif
