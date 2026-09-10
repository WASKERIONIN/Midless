/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS
#include <pthread.h>
#include <math.h>
#include <string.h>
#include "raylib.h"
#include "raygui.h"
#include "screens.h"
#include "chat.h"
#include "player.h"
#include "../hunter.h"
#include "world.h"
#include "block.h"
#include "networkhandler.h"
#include "packet.h"
#include "client.h"
#include "clientws.h"
#include "blockitemrenderer.h"
#include "localserver.h"
#include "settings.h"
#include "mapview.h"
#include "bird.h"
#include "soundfx.h"

Screen currentScreen = SCREEN_LOGIN;
bool screenCursorEnabled = false;
bool screenShowDebug = false;
static bool loadingStarted = false;
static bool loadingFailed = false;
int screenHeight;
int screenWidth;
bool *exitGame;
Color uiColBg;
int maxFPSChoice = 0;
const char* maxFPS = "60";

/* v43: animated cosmic menu backdrop */
#define MENU_STAR_COUNT 150
typedef struct MenuStar {
    float x, y;
    float size;
    float phase;
    float speed;
    Color color;
} MenuStar;
static MenuStar menuStars[MENU_STAR_COUNT];
static bool menuStarsReady = false;

static void InitMenuStars(void) {
    for (int i = 0; i < MENU_STAR_COUNT; i++) {
        menuStars[i].x = (float)(GetRandomValue(0, 10000)) / 10000.0f;
        menuStars[i].y = (float)(GetRandomValue(0, 10000)) / 10000.0f;
        menuStars[i].size = 1.0f + (float)GetRandomValue(0, 220) / 100.0f;
        menuStars[i].phase = (float)GetRandomValue(0, 628) / 100.0f;
        menuStars[i].speed = 0.6f + (float)GetRandomValue(0, 24) / 10.0f;
        int roll = GetRandomValue(0, 99);
        if (roll < 55) menuStars[i].color = (Color){255, 240, 220, 255};
        else if (roll < 82) menuStars[i].color = (Color){168, 216, 255, 255};
        else menuStars[i].color = (Color){236, 150, 255, 255};
    }
    menuStarsReady = true;
}

static void DrawMenuBackground(void) {
    if (!menuStarsReady) InitMenuStars();
    float w = (float)screenWidth, h = (float)screenHeight;
    float t = (float)GetTime();

    /* vertical indigo gradient with a magenta horizon glow */
    DrawRectangleGradientV(0, 0, screenWidth, screenHeight, (Color){10, 4, 26, 255}, (Color){38, 10, 66, 255});
    DrawCircleGradient((int)(w * 0.5f + sinf(t * 0.11f) * w * 0.06f),
                       (int)(h * 1.06f), h * 0.75f,
                       (Color){150, 40, 200, 46}, BLANK);
    DrawCircleGradient((int)(w * 0.16f), (int)(h * 0.22f), h * 0.42f,
                       (Color){40, 190, 210, 26}, BLANK);
    DrawCircleGradient((int)(w * 0.84f), (int)(h * 0.30f), h * 0.38f,
                       (Color){210, 60, 235, 30}, BLANK);

    /* twinkling stars (positions wrap with the window size) */
    for (int i = 0; i < MENU_STAR_COUNT; i++) {
        MenuStar *s = &menuStars[i];
        float twinkle = 0.45f + 0.55f * sinf(t * s->speed + s->phase);
        unsigned char a = (unsigned char)(225 * twinkle);
        float x = s->x * w, y = s->y * h;
        DrawCircleV((Vector2){x, y}, s->size * 0.5f,
                    (Color){s->color.r, s->color.g, s->color.b, a});
        if (s->size > 2.6f) {
            DrawRectangle((int)(x - s->size * 1.8f), (int)y, (int)(s->size * 3.6f), 1,
                          (Color){s->color.r, s->color.g, s->color.b, (unsigned char)(a / 4)});
            DrawRectangle((int)x, (int)(y - s->size * 1.8f), 1, (int)(s->size * 3.6f),
                          (Color){s->color.r, s->color.g, s->color.b, (unsigned char)(a / 4)});
        }
    }

    /* a couple of far island silhouettes drifting by */
    for (int k = 0; k < 3; k++) {
        float drift = fmodf(t * (4.0f + k * 2.5f) + k * 500.0f, w + 260.0f) - 130.0f;
        float iy = h * (0.32f + 0.16f * k) + sinf(t * 0.4f + k * 2.1f) * 6.0f;
        float iw = 74.0f - k * 14.0f, ih = 30.0f - k * 5.0f;
        Color rock = (Color){22, 14, 40, 235};
        Color rim = (Color){80, 220, 190, 90};
        DrawTriangle((Vector2){drift - iw / 2, iy}, (Vector2){drift + iw / 2, iy},
                     (Vector2){drift, iy + ih}, rock);
        DrawLineEx((Vector2){drift - iw / 2, iy}, (Vector2){drift + iw / 2, iy}, 2.0f, rim);
        DrawCircleV((Vector2){drift, iy - 1}, 2.0f, (Color){94, 231, 255, 120});
    }
}

/* button wrapper with UI click blip */
static bool MenuButton(Rectangle bounds, const char *text) {
    bool clicked = GuiButton(bounds, text);
    if (clicked) SoundFx_PlayClick();
    return clicked;
}

static void DrawPanel(Rectangle bounds) {
    DrawRectangleRec(bounds, (Color){12, 5, 28, 168});
    DrawRectangleLinesEx(bounds, 1.0f, (Color){94, 231, 255, 90});
    DrawRectangleLinesEx((Rectangle){bounds.x - 1, bounds.y - 1, bounds.width + 2, bounds.height + 2},
                         1.0f, (Color){200, 60, 255, 50});
}

void Screen_Init(Texture2D terrain, bool *exit) {
    exitGame = exit;
    BlockItemRenderer_Init(terrain);
    maxFPSChoice = gameSettings.maxFpsChoice;
    if (maxFPSChoice == 1) maxFPS = "120";
    else if (maxFPSChoice == 2) maxFPS = "Unlimited";
    else maxFPS = "60";

    //Set UI colors
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL,    0xc86bffff);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,      0x140820aa);
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,      0xe8f6ffff);
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED,   0x5ee7ffff);
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED,     0x2ee6c744);
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED,     0xffffffff);
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED,   0xffb347ff);
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED,     0x4b1d8f88);
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED,     0xffffffff); 

    GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL,    0xfffcfcff); 
    GuiSetStyle(SLIDER, BASE_COLOR_NORMAL,      0x00000000); 
    GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL,      0xffffffff); 
    GuiSetStyle(SLIDER, BORDER_COLOR_FOCUSED,   0xf1f1f1ff); 
    GuiSetStyle(SLIDER, TEXT_COLOR_FOCUSED,     0xf1f1f1ff); 
    GuiSetStyle(SLIDER, BASE_COLOR_PRESSED,     0xfcffffff); 
    GuiSetStyle(SLIDER, BORDER_COLOR_PRESSED,   0xfcffffff); 
    GuiSetStyle(SLIDER, TEXT_COLOR_PRESSED,     0xffffffff); 
    GuiSetStyle(SLIDER, BORDER_WIDTH,           2); 
    
    GuiSetStyle(PROGRESSBAR, BORDER_COLOR_NORMAL,   0xfffdfdff); 
    GuiSetStyle(PROGRESSBAR, BORDER_COLOR_PRESSED,  0xfbf8f8ff); 
    GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED,    0xf8fbfbff); 
    
    GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL,   0xf9f9f9ff); 
    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL,     0xfbfbfbff); 
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL,     0xfdf9f9ff); 
    GuiSetStyle(TEXTBOX, BASE_COLOR_FOCUSED,    0xc7effeff); 
    GuiSetStyle(TEXTBOX, BORDER_COLOR_PRESSED,  0x0392c7ff); 
    GuiSetStyle(TEXTBOX, TEXT_COLOR_PRESSED,    0x338bafff); 
}

void Screen_Shutdown(void) {
    BlockItemRenderer_Shutdown();
}

void Screen_DrawGame(void) {

    //Draw debug infos
    if (screenShowDebug) {
        const char* coordText = TextFormat("X: %i Y: %i Z: %i", (int)player.position.x, (int)player.position.y, (int)player.position.z);
        const char* debugText;

        if (networkConnectedToServer) {
            debugText = TextFormat("%2i FPS %2i PING", GetFPS(), networkPing);
        } else {
            debugText = TextFormat("%2i FPS", GetFPS());
        }
    
        const char* versionText = "Midless Cosmic Edition";
        DrawText(versionText, 9, 9, 20, BLACK);
        DrawText(versionText, 8, 8, 20, WHITE);

        DrawText(debugText, 9, 29, 20, BLACK);
        DrawText(coordText, 9, 49, 20, BLACK);
        DrawText(debugText, 8, 28, 20, WHITE);
        DrawText(coordText, 8, 48, 20, WHITE);

        int birds = 0, fly = 0, sit = 0, peck = 0;
        Bird_GetStats(&birds, &fly, &sit, &peck);
        const char *birdText = TextFormat("Finches: %i fly:%i sit:%i peck:%i", birds, fly, sit, peck);
        DrawText(birdText, 9, 69, 20, BLACK);
        DrawText(birdText, 8, 68, 20, (Color){168, 216, 255, 255});
    }

    if (player.flying) {
        const char *flyText = "FLY MODE  Tab to walk  Space/Shift up/down";
        int flyX = screenWidth / 2 - MeasureText(flyText, 16) / 2;
        DrawText(flyText, flyX + 1, 9, 16, BLACK);
        DrawText(flyText, flyX, 8, 16, (Color){94, 231, 255, 255});
    } else if (currentScreen == SCREEN_GAME) {
        /* v44 traversal hints with dash cooldown */
        double dashLeft = player.dashReadyTime - GetTime();
        const char *moveText = player.webActive
            ? "WEB: hold SHIFT to reel   SPACE release   F detach"
            : (dashLeft > 0.0
                ? TextFormat("F web   SPACE x2 jump   hold SPACE glide   SHIFT dash %.1f", dashLeft)
                : "F web   SPACE x2 jump   hold SPACE glide   SHIFT dash ready");
        int mvX = screenWidth / 2 - MeasureText(moveText, 16) / 2;
        Color mvCol = dashLeft > 0.0 ? (Color){120, 150, 190, 255} : (Color){94, 255, 214, 255};
        DrawText(moveText, mvX + 1, 9, 16, BLACK);
        DrawText(moveText, mvX, 8, 16, mvCol);
    }

    /* v45: vitals HUD - wireframe diamond pips + hunter bounty */
    if (currentScreen == SCREEN_GAME) {
        int hp = Player_GetHp();
        int bx = 8, by = 92, cell = 20;
        for (int i = 0; i < 10; i++) {
            int x = bx + i * cell, y = by;
            Color edge = (i < hp) ? (Color){94, 255, 214, 255} : (Color){90, 90, 110, 160};
            Color fill = (i < hp) ? (Color){94, 255, 214, 70} : BLANK;
            DrawLine(x + 7, y, x + 13, y + 6, edge);
            DrawLine(x + 13, y + 6, x + 7, y + 12, edge);
            DrawLine(x + 7, y + 12, x + 1, y + 6, edge);
            DrawLine(x + 1, y + 6, x + 7, y, edge);
            if (fill.a > 0) {
                DrawLine(x + 7, y + 2, x + 11, y + 6, fill);
                DrawLine(x + 11, y + 6, x + 7, y + 10, fill);
                DrawLine(x + 7, y + 10, x + 3, y + 6, fill);
                DrawLine(x + 3, y + 6, x + 7, y + 2, fill);
                DrawPixelV((Vector2){ x + 7, y + 6 }, edge);
            }
        }
        const char *bountyText = TextFormat("VOID HUNTERS FELLED: %d", Hunter_GetBounty());
        DrawText(bountyText, bx + 1, by + 17, 14, BLACK);
        DrawText(bountyText, bx, by + 16, 14, (Color){200, 160, 255, 220});

        /* hurt flash */
        double sinceHurt = GetTime() - player.lastHurtTime;
        if (sinceHurt < 0.3) {
            DrawRectangle(0, 0, screenWidth, screenHeight,
                          (Color){255, 235, 245, (unsigned char)(70.0f * (1.0 - sinceHurt / 0.3))});
        }
    }

    //Draw crosshair
    DrawRectangle(screenWidth / 2 - 8, screenHeight / 2 - 2, 16, 4, uiColBg);
    DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 + 2,  4, 6, uiColBg);
    DrawRectangle(screenWidth / 2 - 2, screenHeight / 2 - 8,  4, 6, uiColBg);

    // Draw the selected block over a soft glow.
    float glowPulse = 0.5f + 0.5f * sinf((float)GetTime() * 2.0f);
    DrawCircleGradient(screenWidth - 48, 48, 52.0f,
                       (Color){120, 60, 200, (unsigned char)(40 + 24 * glowPulse)}, BLANK);

    BlockItemRenderer_Draw(player.blockSelected, (Rectangle){screenWidth - 88, 8, 80, 80});

    //Draw Chat
    Chat_Draw((Vector2){16, screenHeight - 52}, uiColBg);
    MapView_Draw();

    // void rescue fade
    float fade = Player_GetRespawnFade();
    if (fade > 0.0f) {
        DrawRectangle(0, 0, screenWidth, screenHeight,
                      (Color){8, 3, 20, (unsigned char)(fade * 255.0f)});
    }
}

void Screen_BeginSingleplayer(void) {
    loadingStarted = false;
    loadingFailed = false;
    DisableCursor();
    Screen_Switch(SCREEN_LOADING);
}

void Screen_DrawPause(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){5, 2, 14, 120});

    int offsetY = screenHeight / 2 - 120;
    int offsetX = screenWidth / 2 - 100;
    DrawPanel((Rectangle){offsetX - 14, offsetY - 14, 228, (float)(5 * 35 + 26)});

    int index = 0;

    //Continue Button
    if (MenuButton((Rectangle) {offsetX , offsetY + (index++ * 35), 200, 30 }, "Continue")) {
        Screen_Switch(SCREEN_GAME);
        DisableCursor();
        screenCursorEnabled = false;
        return;
    }

    //Options Button
    if (MenuButton((Rectangle) {offsetX, offsetY + (index++ * 35), 200, 30 }, "Options")) {
        Screen_Switch(SCREEN_OPTIONS);
    }

    if (LocalServer_IsRunning()) {
        if (MenuButton((Rectangle){offsetX, offsetY + (index++ * 35), 200, 30}, "New World")) {
            LocalServer_Stop();
            LocalServer_WipeWorld(false);
            player.flying = false;
            player.blockSelected = 1;
            Bird_Clear();
            MapView_Reset();
            Screen_BeginSingleplayer();
            return;
        }
        if (MenuButton((Rectangle){offsetX, offsetY + (index++ * 35), 200, 30}, "Regenerate World")) {
            LocalServer_Stop();
            LocalServer_WipeWorld(true);
            player.flying = false;
            Bird_Clear();
            MapView_Reset();
            Screen_BeginSingleplayer();
            return;
        }
    }

    //Main Menu Button
    if (MenuButton((Rectangle) {offsetX, offsetY + (index++ * 35), 200, 30 }, "Main Menu")) {
        if (networkConnectedToServer) {
            Network_Disconnect();
        } else {
            Screen_Switch(SCREEN_LOGIN);
             screenCursorEnabled = false;
            World_Clear();
        }
    }

    //Quit Button
    if (MenuButton((Rectangle) {offsetX, offsetY + (index++ * 35), 200, 30 }, "Quit")) {
        *exitGame = true;
    }
}

void Screen_DrawOptions(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){5, 2, 14, 120});

    int offsetY = screenHeight / 2 - 130;
    int offsetX = screenWidth / 2 - 100;
    DrawPanel((Rectangle){offsetX - 14, offsetY - 14, 228, (float)(8 * 35 + 20)});

    const char* drawDistanceTxt = TextFormat("Draw Distance: %i", world.drawDistance);

    //Draw distance Button
    float drawDistanceValue = (float)world.drawDistance;
    GuiSlider((Rectangle) {offsetX, offsetY, 200, 30 }, "", "", &drawDistanceValue, 2, 16);
    int newDrawDistance = (int)(drawDistanceValue + 0.5f);
    Vector2 sizeText = MeasureTextEx(GetFontDefault(), drawDistanceTxt, 10.0f, 1);
    DrawTextEx(GetFontDefault(), drawDistanceTxt, (Vector2){offsetX + 100 - sizeText.x / 2 + 1, offsetY + 15 - sizeText.y / 2 + 1}, 10.0f, 1, BLACK);
    DrawTextEx(GetFontDefault(), drawDistanceTxt, (Vector2){offsetX + 100 - sizeText.x / 2, offsetY + 15 - sizeText.y / 2}, 10.0f, 1, WHITE);

    if (newDrawDistance != world.drawDistance) {
        if (newDrawDistance > world.drawDistance) {
            world.drawDistance = newDrawDistance;
            World_LoadChunks();
        } else {
            world.drawDistance = newDrawDistance;
            World_Reload();
        }
        gameSettings.drawDistance = world.drawDistance;
        Settings_Save();

        if (networkConnectedToServer) {
            Network_Send(Packet_CreateSetDrawDistance(world.drawDistance));
        }
    }

    offsetY += 35;

    //Draw Debug Button
    const char* debugStateTxt = "OFF";
    if (screenShowDebug) debugStateTxt = "ON";
    const char* showDebugTxt = TextFormat("Show Debug: %s", debugStateTxt);
    if (MenuButton((Rectangle) {offsetX, offsetY, 200, 30 }, showDebugTxt)) {
        screenShowDebug = !screenShowDebug;
    }

    offsetY += 35;

    //Draw Max FPS
    const char* maxFPSTxt = TextFormat("Max FPS: %s", maxFPS);
    if (MenuButton((Rectangle) {offsetX, offsetY, 200, 30 }, maxFPSTxt)) {
        maxFPSChoice++;
        if (maxFPSChoice == 3) maxFPSChoice = 0;
        if (maxFPSChoice == 0) {
            maxFPS = "60";
            SetTargetFPS(60);
        } else if (maxFPSChoice == 1) {
            maxFPS = "120";
            SetTargetFPS(120);
        } else if (maxFPSChoice == 2) {
            maxFPS = "Unlimited";
            SetTargetFPS(0);
        }
        gameSettings.maxFpsChoice = maxFPSChoice;
        Settings_Save();
    }

    offsetY += 35;

    const char *fullTxt = TextFormat("Fullscreen: %s  (F11)", gameSettings.fullscreen ? "ON" : "OFF");
    if (MenuButton((Rectangle){offsetX, offsetY, 200, 30}, fullTxt)) {
        Settings_ToggleFullscreen();
    }

    offsetY += 35;

    const char *resTxt = TextFormat("Resolution: %s", Settings_ResolutionLabel());
    if (MenuButton((Rectangle){offsetX, offsetY, 200, 30}, resTxt)) {
        Settings_CycleResolution();
    }

    offsetY += 35;

    float vol = SoundFx_GetVolume();
    GuiSlider((Rectangle){offsetX, offsetY, 200, 30}, "", "", &vol, 0.0f, 1.0f);
    const char *volTxt = TextFormat("Volume: %i%%", (int)(vol * 100.0f + 0.5f));
    Vector2 volSize = MeasureTextEx(GetFontDefault(), volTxt, 10.0f, 1);
    DrawTextEx(GetFontDefault(), volTxt,
               (Vector2){offsetX + 100 - volSize.x / 2 + 1, offsetY + 15 - volSize.y / 2 + 1}, 10.0f, 1, BLACK);
    DrawTextEx(GetFontDefault(), volTxt,
               (Vector2){offsetX + 100 - volSize.x / 2, offsetY + 15 - volSize.y / 2}, 10.0f, 1, WHITE);
    if (fabsf(vol - SoundFx_GetVolume()) > 0.001f) {
        SoundFx_SetVolume(vol);
        gameSettings.volume = (int)(vol * 100.0f + 0.5f);
        Settings_Save();
    }

    offsetY += 35;

    if (MenuButton((Rectangle) {offsetX, offsetY, 200, 30 }, "Back")) {
        Screen_Switch(SCREEN_PAUSE);
    }

}

void Screen_DrawJoining(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);
    DrawText("Joining Server...", screenWidth / 2 - 80, screenHeight / 2 - 30, 20, WHITE);
}

char nameInput[16] = "Player";
char ipInput[128] = "localhost";
char portInput[5] = "25565";

bool loginEditMode = false;
bool ipEditMode = false;
bool portEditMode = false;

void Screen_DrawLogin(void) {
    if(IsCursorHidden()) EnableCursor();
    DrawMenuBackground();

    const char *title = "MIDLESS";
    const char *subtitle = "COSMIC EDITION";
    int offsetY = screenHeight / 2;
    int offsetX = screenWidth / 2;
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 1.6f);

    DrawPanel((Rectangle){(float)offsetX - 94, (float)offsetY - 22, 188, 130});

    /* glow layering under the title */
    for (int layer = 3; layer >= 1; layer--) {
        Color glow = (Color){150, 40, 220, (unsigned char)(26 * layer + 8 * pulse * layer)};
        DrawText(title, offsetX - (MeasureText(title, 80) / 2), offsetY - 120 + layer, 80, glow);
    }
    DrawText(title, offsetX - (MeasureText(title, 80) / 2) + 2, offsetY - 118, 80, (Color){40, 10, 70, 255});
    DrawText(title, offsetX - (MeasureText(title, 80) / 2), offsetY - 120, 80, (Color){232, 120, 255, 255});
    DrawText(subtitle, offsetX - (MeasureText(subtitle, 20) / 2), offsetY - 38, 20, (Color){94, 231, 255, 255});

    const char *hint = "WASD move - Space jump - Tab fly - M map - T chat - F5 camera";
    DrawText(hint, offsetX - MeasureText(hint, 12) / 2, screenHeight - 26, 12,
             (Color){150, 160, 200, 200});
    const char *tag = "floating islands - starlit void - the sun is a black hole";
    DrawText(tag, offsetX - MeasureText(tag, 12) / 2, screenHeight - 44, 12,
             (Color){190, 120, 230, 180});

    //Name Input
    if (GuiTextBox((Rectangle) { offsetX - 80, offsetY - 15, 160, 30 }, nameInput, 16, loginEditMode)) {
        loginEditMode = !loginEditMode;
    }

    //IP Input
    if (GuiTextBox((Rectangle) { offsetX - 80, offsetY + 20, 116, 30 }, ipInput, 128, ipEditMode)) {
        ipEditMode = !ipEditMode;
    }

    //Port Input
    if (GuiTextBox((Rectangle) { offsetX + 40, offsetY + 20, 40, 30 }, portInput, 5, portEditMode)) {
        portEditMode = !portEditMode;
    }

    //Login button
    if (MenuButton((Rectangle) { offsetX - 80, offsetY + 55, 160, 30 }, "Login")) {
        DisableCursor();
        Screen_Switch(SCREEN_JOINING);
        networkThreadState = 0;
        networkName = nameInput;
        networkIp = ipInput;
        networkPort = TextToInteger(portInput);

        char fullAddress[128] = "";
        strcat(fullAddress, ipInput);
        strcat(fullAddress, ":");
        strcat(fullAddress, portInput);

        networkFullAddress = fullAddress;

        //Start Client on a new thread
        pthread_t clientThreadId;

        #if !defined(PLATFORM_WEB)
            pthread_create(&clientThreadId, NULL, Client_Init, (void*)&networkThreadState);
        #else
            pthread_create(&clientThreadId, NULL, ClientWs_Init, (void*)&networkThreadState);
        #endif
    }
    
    //Singleplayer Button
    if (MenuButton((Rectangle) { offsetX - 80, offsetY + 90, 160, 30 }, "Singleplayer")) {
        Screen_BeginSingleplayer();
    }

}

void Screen_DrawLoading(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){10, 6, 24, 255});

    if (!loadingStarted) {
        loadingStarted = true;
        loadingFailed = !World_LoadSingleplayer();
        if (loadingFailed) EnableCursor();
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        LocalServer_Stop();
        loadingStarted = false;
        loadingFailed = false;
        EnableCursor();
        Screen_Switch(SCREEN_LOGIN);
        return;
    }

    if (loadingFailed) {
        const char *err = "Could not start the world.";
        const char *hint = "Delete the 'world' folder next to game.exe, then try again.";
        DrawText(err, screenWidth / 2 - MeasureText(err, 20) / 2, screenHeight / 2 - 30, 20, WHITE);
        DrawText(hint, screenWidth / 2 - MeasureText(hint, 16) / 2, screenHeight / 2 + 8, 16,
                 (Color){180, 180, 200, 255});
        if (MenuButton((Rectangle){screenWidth / 2 - 80, screenHeight / 2 + 50, 160, 30}, "Back")) {
            loadingStarted = false;
            loadingFailed = false;
            EnableCursor();
            Screen_Switch(SCREEN_LOGIN);
        }
        return;
    }

    DrawText("Loading World...", screenWidth / 2 - 90, screenHeight / 2, 20, WHITE);
    DrawText("ESC to cancel", screenWidth / 2 - 70, screenHeight / 2 + 28, 16,
             (Color){160, 160, 180, 255});
}

void Screen_Draw(void) {
    screenHeight = GetScreenHeight();
    screenWidth = GetScreenWidth();
    
    uiColBg = (Color){ 0, 0, 0, 80 };
    
    if (currentScreen == SCREEN_GAME)
        Screen_DrawGame();
    else if (currentScreen == SCREEN_PAUSE)
        Screen_DrawPause();
    else if(currentScreen == SCREEN_LOADING)
        Screen_DrawLoading();
    else if (currentScreen == SCREEN_JOINING)
        Screen_DrawJoining();
    else if (currentScreen == SCREEN_LOGIN)
        Screen_DrawLogin();
    else if (currentScreen == SCREEN_OPTIONS)
        Screen_DrawOptions();
}

void Screen_Switch(Screen screen) {
    currentScreen = screen;
}
