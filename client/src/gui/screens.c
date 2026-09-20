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
#include "i18n.h"
#include "raygui.h"
#include "golem.h"
#include "screens.h"
#include "mapedit.h"
#include "parkourmap.h"
#include "chat.h"
#include "player.h"
#include "../hunter.h"
#include "../mobs.h"
#include "world.h"
#include "block.h"
#include "networkhandler.h"
#include "textures.h"
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

/* v49.1: laser upgrade menu state */
static bool upgradeMenuOpen = false;
static double upgradeMenuToggleTime = -10.0;
/* v53: the satchel - a passive look at what you carry. Same-frame lesson
 * from v49.4 applies: the toggle key is read ONLY in the input pass. */
static bool inventoryOpen = false;
static double inventoryToggleTime = 0.0;

void Screens_UpgradeMenuToggle(void) {
    /* v49.3: IsKeyPressed stays true for the whole frame - without this
     * debounce the draw code re-read it and closed the menu the same frame
     * (the "B does nothing, camera spins" bug). */
    if (GetTime() - upgradeMenuToggleTime < 0.3) return;
    upgradeMenuToggleTime = GetTime();
    upgradeMenuOpen = !upgradeMenuOpen;
    inventoryOpen = false;             /* the panels are exclusive */
    if (upgradeMenuOpen) {
        EnableCursor();
        screenCursorEnabled = true;
    } else {
        DisableCursor();
        screenCursorEnabled = false;
    }
}

bool Screens_UpgradeMenuIsOpen(void) { return upgradeMenuOpen; }

/* v55: hand-drawn buttons with hover feedback - consistent 18px labels,
 * disabled state, palette borders. Replaces the tiny raygui defaults in
 * the game-facing menus. */
static bool CosmicButton(Rectangle bounds, const char *label, bool enabled) {
    Vector2 mp = GetMousePosition();
    /* v64.0: the hover/click area is 3px larger on every side than the
     * drawing - near-the-edge cursors no longer "miss" a button that
     * is visibly under them */
    Rectangle hit = { bounds.x - 3, bounds.y - 3, bounds.width + 6, bounds.height + 6 };
    bool hover = enabled && CheckCollisionPointRec(mp, hit);
    DrawRectangleRec(bounds, enabled ? (Color){ 24, 12, 48, 235 } : (Color){ 14, 9, 26, 210 });
    DrawRectangleLinesEx(bounds, hover ? 2 : 1,
        !enabled ? (Color){ 90, 92, 120, 150 } :
        hover   ? (Color){ 96, 255, 214, 255 } : (Color){ 94, 231, 255, 110 });
    int fs = 18;
    Vector2 tb = I18n_MeasureEx(label, fs);
    Color tc = !enabled ? (Color){ 125, 125, 150, 255 }
             : hover   ? (Color){ 225, 255, 250, 255 }
                       : (Color){ 170, 235, 225, 255 };
    I18n_DrawText(label, (int)(bounds.x + bounds.width / 2.0f - tb.x / 2.0f),
             (int)(bounds.y + bounds.height / 2.0f - tb.y / 2.0f), fs, tc);
    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void CosmicCell(int x, int y, int size, bool highlighted) {
    DrawRectangle(x, y, size, size, (Color){ 14, 8, 30, 235 });
    Color border = highlighted ? (Color){ 96, 255, 214, 230 } : (Color){ 94, 231, 255, 85 };
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)size, (float)size }, 1, border);
    DrawRectangleLinesEx((Rectangle){ (float)x - 1, (float)y - 1, (float)size + 2, (float)size + 2 }, 1,
                         (Color){ 200, 60, 255, 45 });
}

/* v65.6: satchel names what it shows (hover a cell to read the name) */
static const char *Satchel_ItemName(int tile) {
    switch (tile) {
        case 73: return "Void Glowcaps";
        case 74: return "Cinder Trumpet";
        case 75: return "Frost Puffball";
        case 40: return "Gaze Scroll";
        default: return "Unknown";
    }
}

/* atlas tile drawn as an inventory icon (crisp point-scaled pixel art) */
static void CosmicTileIcon(Texture2D atlas, int tile, int x, int y, int size) {
    if (atlas.id == 0) return;
    Rectangle src = { (float)((tile % 16) * 16), (float)((tile / 16) * 16), 16.0f, 16.0f };
    Rectangle dst = { (float)x, (float)y, (float)size, (float)size };
    DrawTexturePro(atlas, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

static void CosmicPips(int cx, int cy, int lvl) {
    for (int i = 0; i < 3; i++) {
        int px = cx - 23 + i * 16;
        if (i < lvl) {
            DrawRectangle(px, cy, 12, 6, (Color){ 96, 255, 214, 255 });
            DrawRectangleLinesEx((Rectangle){ (float)px, (float)cy, 12, 6 }, 1, (Color){ 220, 255, 250, 200 });
        } else {
            DrawRectangleLinesEx((Rectangle){ (float)px, (float)cy, 12, 6 }, 1, (Color){ 120, 140, 170, 170 });
        }
    }
}

void Screens_InventoryToggle(void) {
    if (GetTime() - inventoryToggleTime < 0.3) return;
    inventoryToggleTime = GetTime();
    inventoryOpen = !inventoryOpen;
    upgradeMenuOpen = false;           /* the panels are exclusive */
    if (inventoryOpen) {
        EnableCursor();
        screenCursorEnabled = true;
    } else {
        DisableCursor();
        screenCursorEnabled = false;
    }
}

bool Screens_InventoryIsOpen(void) { return inventoryOpen; }
bool screenShowDebug = false;
bool screenShowWorkView = false;      /* v65.3: F4 flora work view */
static bool hostPanelOpen = false;    /* v65.7: F6 host info panel */
void Screens_HostPanelToggle(void) { hostPanelOpen = !hostPanelOpen; }
bool Screens_HostPanelIsOpen(void) { return hostPanelOpen; }

/* v65.7: host settings on the login screen - persisted to server.ini */
static char hostPortInput[8] = "25565";
static char hostMaxInput[4] = "8";
static char hostNameInput[48] = "Midless Cosmic Server";
static bool hostPortEdit = false, hostMaxEdit = false, hostNameEdit = false;
static bool hostCfgLoaded = false;
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
    /* v55: raygui defaults to a 10px font - the reason every menu felt tiny */
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
    GuiSetStyle(BUTTON, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
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

/* v58: quick-slot state shared between the satchel and the HUD strip */
static int satchelPick = -1;            /* item tile picked in the satchel */
static Rectangle hotbarRects[6];   /* v65.6: six quick slots */
static bool hotbarRectsReady = false;

bool Screens_HotbarConsumeClick(void) {
    if (!hotbarRectsReady) return false;
    Vector2 mp = GetMousePosition();
    for (int i = 0; i < 6; i++)
        if (CheckCollisionPointRec(mp, hotbarRects[i])) return true;
    return false;
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
        I18n_DrawText(versionText, 9, 9, 20, BLACK);
        I18n_DrawText(versionText, 8, 8, 20, WHITE);

        I18n_DrawText(debugText, 9, 29, 20, BLACK);
        I18n_DrawText(coordText, 9, 49, 20, BLACK);
        I18n_DrawText(debugText, 8, 28, 20, WHITE);
        I18n_DrawText(coordText, 8, 48, 20, WHITE);

        int birds = 0, fly = 0, sit = 0, peck = 0;
        Bird_GetStats(&birds, &fly, &sit, &peck);
        const char *birdText = TextFormat("Finches: %i fly:%i sit:%i peck:%i", birds, fly, sit, peck);
        I18n_DrawText(birdText, 9, 69, 20, BLACK);
        I18n_DrawText(birdText, 8, 68, 20, (Color){168, 216, 255, 255});

        /* v65.3: flora work stats - what the billboard pass cost this frame
         * and how loaded the heaviest chunk list is against the 1024 cap */
        int wp = 0, wq = 0, wm = 0;
        World_GetWorkStats(&wp, &wq, &wm);
        const char *workText = TextFormat(
            "Flora work: %i plants %i quads, worst chunk %i/1024  [F4 work view: %s]",
            wp, wq, wm, screenShowWorkView ? "ON" : "off");
        I18n_DrawText(workText, 9, 89, 20, BLACK);
        I18n_DrawText(workText, 8, 88, 20, (Color){255, 214, 120, 255});
    }

    if (player.flying) {
        const char *flyText = Tr("FLY MODE  Tab to walk  Space/Shift up/down");
        int flyX = screenWidth / 2 - I18n_MeasureText(flyText, 19) / 2;
        I18n_DrawText(flyText, flyX + 1, 10, 19, BLACK);
        I18n_DrawText(flyText, flyX, 9, 19, (Color){94, 231, 255, 255});
    } else if (currentScreen == SCREEN_GAME) {
        /* v44 traversal hints with dash cooldown */
        double dashLeft = player.dashReadyTime - GetTime();
        int dashCharges = 2 - player.dashChargesUsed;
        Vector3 padCheck = { player.position.x, player.position.y - 0.1f, player.position.z };
        const char *weaponTag = player.weaponMode ? "[R: LASER]" : "[R: BLADE]";
        const char *moveText;
        if (player.webActive)
            moveText = TextFormat(Tr("%s WEB: hold SHIFT to reel   SPACE release   F detach"), weaponTag);
        else if (Player_NearWarpCore())
            moveText = TextFormat(Tr("%s E - WARP   B - UPGRADE LASER (5 shards)"), weaponTag);
        else if (Mobs_GetMushrooms() > 0)
            moveText = TextFormat(Tr("%s MUSHROOM x%d - pin it to a quick slot to eat   I - SATCHEL"),
                                  weaponTag, Mobs_GetMushrooms());
        else if (World_GetBlock(padCheck) == 21)
            moveText = TextFormat(Tr("%s SPACE - LAUNCH from the pad"), weaponTag);
        else if (dashCharges > 0)
            moveText = TextFormat(Tr("%s F web   SPACE x2 jump   glide   SHIFT dash x%d   I - SATCHEL"), weaponTag, dashCharges);
        else
            moveText = TextFormat(Tr("%s dash recharges %.1f   I - SATCHEL"), weaponTag, dashLeft);
        int mvX = screenWidth / 2 - I18n_MeasureText(moveText, 19) / 2;
        Color mvCol = (player.webActive || dashCharges > 0) ? (Color){94, 255, 214, 255} : (Color){120, 150, 190, 255};
        I18n_DrawText(moveText, mvX + 1, 10, 19, BLACK);
        I18n_DrawText(moveText, mvX, 9, 19, mvCol);
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
        const char *bountyText = TextFormat(Tr("VOID HUNTERS FELLED: %d"), Hunter_GetBounty());
        I18n_DrawText(bountyText, bx + 1, by + 22, 21, BLACK);
        I18n_DrawText(bountyText, bx, by + 21, 21, (Color){200, 160, 255, 220});
        Golem_DrawHUD();   /* v65.21: Warden boss bar */

        /* v48: shard counter with a tiny wireframe diamond */
        int shards = Player_GetShards();
        const char *shardText = TextFormat(Tr("VOID SHARDS: %d"), shards);
        int sy = by + 48;
        Color shardCol = shards > 0 ? (Color){96, 255, 214, 255} : (Color){120, 120, 140, 220};
        I18n_DrawText(shardText, bx + 1, sy + 1, 18, BLACK);
        I18n_DrawText(shardText, bx, sy, 18, shardCol);
        Color dEdge = shardCol;
        DrawLine(bx + 128, sy + 3, bx + 132, sy + 7, dEdge);
        DrawLine(bx + 132, sy + 7, bx + 128, sy + 11, dEdge);
        DrawLine(bx + 128, sy + 11, bx + 124, sy + 7, dEdge);
        DrawLine(bx + 124, sy + 7, bx + 128, sy + 3, dEdge);

        /* v65.7: while hosting, the address lives on screen at all times */
        if (LocalServer_IsRunning()) {
            const char *hostLine = TextFormat(Tr("HOST %s:%d  players %d/%d"),
                                              LocalServer_GetLocalIp(), LocalServer_GetPort(),
                                              LocalServer_GetPlayerCount(), LocalServer_GetMaxPlayers());
            I18n_DrawText(hostLine, bx + 1, sy + 25, 16, BLACK);
            I18n_DrawText(hostLine, bx, sy + 24, 16, (Color){ 120, 255, 214, 235 });
        }

        /* v59.8: the radio station name, BOTTOM right corner - the top
         * right corner belongs to the block preview */
        {
            const char *stText = TextFormat("\xe2\x99\xaa %s", Tr(SoundFx_TrackName()));
            int stw = I18n_MeasureText(stText, 19);
            int stx = screenWidth - stw - 16;
            int sty = screenHeight - 32;
            I18n_DrawText(stText, stx + 1, sty + 1, 19, BLACK);
            I18n_DrawText(stText, stx, sty, 19, (Color){255, 214, 130, 235});
        }

        /* v49.1: the edge vignette is gone - the tide speaks through the
         * banner, the hunters' red glow and the swelling drone */
        float tideIncoming = Hunter_GetCalmTimeLeft();
        if (Hunter_GetSurgeTimeLeft() > 0.0f) {
            float pulse = 0.75f + 0.25f * sinf(GetTime() * 6.0f);
            const char *tideText = TextFormat(Tr("VOID TIDE  %.0f"), Hunter_GetSurgeTimeLeft());
            int tx = screenWidth / 2 - I18n_MeasureText(tideText, 30) / 2;
            I18n_DrawText(tideText, tx + 2, 44, 30, BLACK);
            I18n_DrawText(tideText, tx, 42, 30, (Color){255, 90, 120, (unsigned char)(255.0f * pulse)});
        } else if (tideIncoming > 0.0) {
            const char *warnText = TextFormat(Tr("THE VOID STIRS - TIDE IN %.0f"), tideIncoming);
            int wx = screenWidth / 2 - I18n_MeasureText(warnText, 22) / 2;
            I18n_DrawText(warnText, wx + 1, 45, 22, BLACK);
            I18n_DrawText(warnText, wx, 44, 22, (Color){255, 190, 110, 230});
        }

        /* hurt flash */
        double sinceHurt = GetTime() - player.lastHurtTime;
        if (sinceHurt < 0.3) {
            DrawRectangle(0, 0, screenWidth, screenHeight,
                          (Color){255, 235, 245, (unsigned char)(70.0f * (1.0 - sinceHurt / 0.3))});
        }
    }

    /* v65.7: F6 host panel - everything a friend needs to join */
    if (hostPanelOpen && LocalServer_IsRunning()) {
        int px = screenWidth / 2 - 260, py = screenHeight / 2 - 130;
        DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 8, 3, 16, 165 });
        DrawPanel((Rectangle){ (float)px - 14, (float)py - 14, 548, 288 });
        I18n_DrawText("HOST PANEL", px + 2, py + 3, 26, BLACK);
        I18n_DrawText("HOST PANEL", px, py, 26, (Color){ 96, 255, 214, 255 });
        const char *nm = TextFormat(Tr("server: %s"), LocalServer_GetName());
        I18n_DrawText(nm, px, py + 40, 18, (Color){ 225, 245, 255, 255 });
        const char *lan = Tr("friends on your LAN join at this address:");
        I18n_DrawText(lan, px, py + 72, 16, (Color){ 150, 160, 200, 255 });
        const char *addr = TextFormat("%s:%d", LocalServer_GetLocalIp(), LocalServer_GetPort());
        I18n_DrawText(addr, px + 2, py + 96 + 2, 30, BLACK);
        I18n_DrawText(addr, px, py + 96, 30, (Color){ 255, 214, 130, 255 });
        const char *wan = TextFormat(Tr("friends over the internet: forward port %d on your router to this PC"),
                                     LocalServer_GetPort());
        I18n_DrawText(wan, px, py + 140, 16, (Color){ 170, 170, 195, 255 });
        const char *pl = TextFormat(Tr("players online: %d of %d"),
                                    LocalServer_GetPlayerCount(), LocalServer_GetMaxPlayers());
        I18n_DrawText(pl, px, py + 168, 18, (Color){ 120, 255, 214, 255 });
        I18n_DrawText("server.ini next to the game edits port / max players / name",
                      px, py + 200, 14, (Color){ 130, 130, 160, 255 });
        I18n_DrawText("F6 - close", px, py + 224, 14, (Color){ 140, 140, 165, 255 });
    }

    /* v49.1/v55: the warp core forge - upgrade cards with sockets */
    if (upgradeMenuOpen) {
        if (!screenCursorEnabled || currentScreen != SCREEN_GAME) {
            upgradeMenuOpen = false;
        } else {
            int mx = screenWidth / 2 - 360;
            int my = screenHeight / 2 - 224;
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 8, 3, 16, 165 });
            DrawPanel((Rectangle){ (float)mx - 16, (float)my - 16, 752, 480 });

            const char *title = "WARP CORE FORGE";
            I18n_DrawText(title, mx + 2, my + 3, 28, BLACK);
            I18n_DrawText(title, mx, my, 28, (Color){ 96, 255, 214, 255 });
            DrawLineEx((Vector2){ (float)mx, (float)my + 40 }, (Vector2){ (float)(mx + 720), (float)my + 40 },
                       1, (Color){ 94, 231, 255, 60 });

            /* shard chip, top right */
            CosmicTileIcon(World_GetTerrainTexture(), 35, mx + 620, my + 8, 28);
            const char *shardLine = TextFormat("x %d", Player_GetShards());
            I18n_DrawText(shardLine, mx + 654, my + 14, 20, (Color){ 200, 160, 255, 255 });

            int rl = Player_GetLaserRangeLvl();
            int lv = Player_GetLaserRateLvl();
            int bl = Player_GetBurstLvl();
            int cl = Player_GetCoolLvl();
            struct {
                const char *name; const char *effect; int lvl; int kind;
            } cards[5] = {
                { "LENS",  "+6 m laser range",            rl, 0 },
                { "COIL",  "faster shots",                lv, 1 },
                { "BURST", "hold = volleys, endless at max", bl, 2 },
                { "COOL",  "cooler coil, quicker shots",  cl, 3 },
                { "ARMOR", "v58: every hit hurts 12% less", Player_GetArmorLvl(), 4 },
            };

            int y0 = my + 50;
            for (int c = 0; c < 5; c++) {
                int cy = y0 + c * 76;
                bool maxed = cards[c].lvl >= 3;

                /* track socket cells */
                for (int p = 0; p < 3; p++) {
                    int px = mx + 430 + p * 40;
                    if (p < cards[c].lvl) {
                        DrawRectangle(px, cy + 12, 34, 18, (Color){ 30, 90, 80, 235 });
                        DrawRectangle(px + 2, cy + 14, 30, 14, (Color){ 96, 255, 214, 255 });
                    } else {
                        DrawRectangle(px, cy + 12, 34, 18, (Color){ 14, 8, 30, 235 });
                    }
                    DrawRectangleLinesEx((Rectangle){ (float)px, (float)cy + 12, 34, 18 }, 1,
                                         p < cards[c].lvl ? (Color){ 220, 255, 250, 180 }
                                                          : (Color){ 94, 231, 255, 70 });
                }

                /* icon socket */
                CosmicCell(mx + 24, cy + 1, 52, maxed);
                int icx = mx + 50, icy = cy + 27;
                if (cards[c].kind == 0) {          /* lens */
                    DrawCircleLines(icx, icy, 16, (Color){ 96, 255, 214, 255 });
                    DrawCircleLines(icx, icy, 9, (Color){ 160, 245, 235, 220 });
                    DrawCircle(icx, icy, 4, (Color){ 225, 255, 250, 255 });
                } else if (cards[c].kind == 1) {   /* coil */
                    DrawCircleLines(icx, icy, 17, (Color){ 255, 190, 84, 255 });
                    DrawCircleLines(icx, icy, 11, (Color){ 255, 220, 140, 230 });
                    DrawCircleLines(icx, icy, 5, (Color){ 255, 240, 190, 210 });
                } else if (cards[c].kind == 2) {   /* burst */
                    DrawCircle(icx - 11, icy + 6, 4, (Color){ 232, 84, 240, 255 });
                    DrawCircle(icx, icy - 4, 4, (Color){ 255, 150, 250, 255 });
                    DrawCircle(icx + 11, icy + 6, 4, (Color){ 148, 64, 255, 255 });
                } else if (cards[c].kind == 3) {    /* cool */
                    for (int a = 0; a < 6; a++) {
                        float an = 3.1416f * a / 3.0f;
                        DrawLineEx((Vector2){ icx - cosf(an) * 15, icy - sinf(an) * 15 },
                                   (Vector2){ icx + cosf(an) * 15, icy + sinf(an) * 15 },
                                   2.0f, (Color){ 110, 230, 255, 255 });
                    }
                    DrawCircle(icx, icy, 4, (Color){ 220, 250, 255, 255 });
                } else {                            /* v58: armor plate */
                    DrawRectangle(icx - 13, icy - 12, 26, 18, (Color){ 70, 80, 120, 235 });
                    DrawRectangleLinesEx((Rectangle){ (float)icx - 13, (float)icy - 12, 26, 18 }, 2,
                                         (Color){ 190, 205, 255, 255 });
                    DrawRectangle(icx - 7, icy + 6, 14, 6, (Color){ 70, 80, 120, 235 });
                    DrawRectangleLinesEx((Rectangle){ (float)icx - 7, (float)icy + 6, 14, 6 }, 1,
                                         (Color){ 190, 205, 255, 200 });
                    DrawCircle(icx, icy - 3, 3, (Color){ 120, 255, 214, 255 });
                }

                /* name + effect */
                I18n_DrawText(cards[c].name, mx + 104, cy + 2, 22, maxed ? (Color){ 120, 190, 170, 255 }
                                                                     : (Color){ 225, 245, 255, 255 });
                I18n_DrawText(cards[c].effect, mx + 104, cy + 30, 14, (Color){ 165, 165, 190, 255 });

                /* buy button */
                if (maxed) {
                    CosmicButton((Rectangle){ (float)(mx + 576), (float)(cy + 2), 124, 42 }, "MAX", false);
                } else {
                    char btxt[48];
                    snprintf(btxt, sizeof(btxt), "FORGE  5");
                    if (CosmicButton((Rectangle){ (float)(mx + 576), (float)(cy + 2), 124, 42 }, btxt,
                                     Player_GetShards() >= 5)) {
                        if (cards[c].kind == 4) Player_BuyArmorUpgrade();
                        else Player_BuyLaserUpgrade(cards[c].kind);
                    }
                }
                if (c < 4)
                    DrawLineEx((Vector2){ (float)(mx + 24), (float)(cy + 68) },
                               (Vector2){ (float)(mx + 700), (float)(cy + 68) }, 1, (Color){ 94, 231, 255, 30 });
            }

            const char *hint = "B / ESC - close      falling or dying burns out one upgrade";
            I18n_DrawText(hint, mx, my + 442, 14, (Color){ 150, 150, 175, 255 });
        }
    }

    /* v55: the satchel - item grid on the left, details on the right */
    if (inventoryOpen) {
        if (!screenCursorEnabled || currentScreen != SCREEN_GAME) {
            inventoryOpen = false;
        } else {
            int mx = screenWidth / 2 - 352;
            int my = screenHeight / 2 - 220;
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 8, 3, 16, 165 });
            /* v65.5: taller panel - 12 slots, currency section, roomier type */
            DrawPanel((Rectangle){ (float)mx - 16, (float)my - 16, 736, 470 });

            const char *title = "VOID SATCHEL";
            I18n_DrawText(title, mx + 2, my + 3, 26, BLACK);
            I18n_DrawText(title, mx, my, 26, (Color){ 96, 255, 214, 255 });

            Texture2D atlas = World_GetTerrainTexture();

            /* ---- v65.5: currency section (top right) - shards are money,
             * they never occupy a satchel slot ---- */
            {
                int curX = mx + 520;
                I18n_DrawText("CURRENCY", curX + 2, my + 5, 16, BLACK);
                I18n_DrawText("CURRENCY", curX, my + 3, 16, (Color){ 120, 190, 175, 255 });
                CosmicTileIcon(atlas, 35, curX + 100, my + 1, 26);
                const char *curCnt = TextFormat("x %d", Player_GetShards());
                I18n_DrawText(curCnt, curX + 132, my + 4, 20, (Color){ 200, 160, 255, 255 });
                DrawLineEx((Vector2){ (float)(mx + 500), (float)(my + 34) },
                           (Vector2){ (float)(mx + 716), (float)(my + 34) }, 1,
                           (Color){ 94, 231, 255, 45 });
            }

            /* ---- item grid (left): ONLY what the player actually carries.
             * v65.5: mushroom species keep their own stacks (glowcaps never
             * fold into puffballs); nothing pre-exists with a zero count ---- */
            int gx = mx + 24, gy = my + 52;
            int cell = 72, gap = 12;
            struct { int tile; int count; int pin; } items[12];
            int nItems = 0;
            const int spTiles[3] = { 73, 74, 75 };
            for (int k = 0; k < 3; k++) {
                int c = Mobs_GetMushroomSpecies(spTiles[k]);
                if (c > 0 && nItems < 12) {
                    items[nItems].tile = spTiles[k];
                    items[nItems].count = c;
                    items[nItems].pin = spTiles[k];   /* v65.6: pin the species art */
                    nItems++;
                }
            }
            if (Player_GetScrollCount() > 0 && nItems < 12) {
                items[nItems].tile = 40;
                items[nItems].count = Player_GetScrollCount();
                items[nItems].pin = 40;
                nItems++;
            }
            for (int i = nItems; i < 12; i++) { items[i].tile = -1; items[i].count = 0; items[i].pin = -1; }
            int hoverTile = -1;
            Vector2 smp = GetMousePosition();
            for (int i = 0; i < 12; i++) {
                int cx = gx + (i % 4) * (cell + gap);
                int cy = gy + (i / 4) * (cell + gap);
                CosmicCell(cx, cy, cell, items[i].tile >= 0);
                if (items[i].tile >= 0) {
                    /* v65.6: the satchel names what you point at */
                    if (CheckCollisionPointRec(smp, (Rectangle){ (float)cx, (float)cy, (float)cell, (float)cell }))
                        hoverTile = items[i].tile;
                    CosmicTileIcon(atlas, items[i].tile, cx + 10, cy + 8, 52);
                    const char *cnt = TextFormat("%d", items[i].count);
                    int w = I18n_MeasureText(cnt, 18);
                    I18n_DrawText(cnt, cx + cell - w - 6 + 1, cy + cell - 22 + 1, 18, BLACK);
                    I18n_DrawText(cnt, cx + cell - w - 6, cy + cell - 22, 18, (Color){ 255, 240, 200, 255 });
                    /* v58: click an item to pin it to a quick slot */
                    if (satchelPick == items[i].pin)
                        DrawRectangleLinesEx((Rectangle){ (float)cx, (float)cy, (float)cell, (float)cell }, 2,
                                             (Color){ 120, 255, 214, 255 });
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                        CheckCollisionPointRec(GetMousePosition(), (Rectangle){ (float)cx, (float)cy, (float)cell, (float)cell })) {
                        satchelPick = items[i].pin;
                        SoundFx_PlayClick();
                    }
                }
            }
            if (hoverTile >= 0) {
                I18n_DrawText(Satchel_ItemName(hoverTile), gx + 2, gy + 3 * cell + 2 * gap + 8,
                              16, (Color){ 255, 240, 200, 255 });
            } else {
                I18n_DrawText("only what you carry shows here", gx + 2, gy + 3 * cell + 2 * gap + 8,
                              14, (Color){ 130, 130, 160, 255 });
            }

            /* ---- details (right side) ---- */
            int sx = gx + 4 * (cell + gap) + 26;
            DrawLineEx((Vector2){ (float)(sx - 14), (float)gy - 6 },
                       (Vector2){ (float)(sx - 14), (float)gy + 2 * cell + gap + 2 }, 1,
                       (Color){ 94, 231, 255, 45 });

            I18n_DrawText("LASER FORGE", sx, gy - 4, 15, (Color){ 120, 190, 175, 255 });
            struct { const char *name; int lvl; } ups[5] = {
                { "LENS", Player_GetLaserRangeLvl() },
                { "COIL", Player_GetLaserRateLvl() },
                { "BURST", Player_GetBurstLvl() },
                { "COOL", Player_GetCoolLvl() },
                { "ARMOR", Player_GetArmorLvl() },
            };
            for (int u = 0; u < 5; u++) {
                int uy = gy + 20 + u * 34;
                I18n_DrawText(ups[u].name, sx, uy + 2, 17, ups[u].lvl >= 3 ? (Color){ 120, 190, 170, 255 }
                                                                      : (Color){ 225, 245, 255, 255 });
                CosmicPips(sx + 90, uy + 6, ups[u].lvl);
                const char *lv = TextFormat("%d/3", ups[u].lvl);
                I18n_DrawText(lv, sx + 172, uy + 2, 15, (Color){ 150, 150, 180, 255 });
            }
            I18n_DrawText("forge at warp cores - B, 5 shards each", sx, gy + 192, 14, (Color){ 140, 140, 165, 255 });

            DrawLineEx((Vector2){ (float)sx, (float)gy + 214 }, (Vector2){ (float)(sx + 270), (float)gy + 214 }, 1,
                       (Color){ 94, 231, 255, 45 });
            I18n_DrawText("FIELD LOG", sx, gy + 224, 15, (Color){ 120, 190, 175, 255 });
            const char *log1 = TextFormat(Tr("Hunters felled: %d"), Hunter_GetBounty());
            I18n_DrawText(log1, sx, gy + 246, 16, (Color){ 255, 200, 120, 255 });
            const char *log2 = TextFormat(Tr("Nearby: %d crawlers, %d wisps, %d spiders"),
                                          Mobs_CrawlerCount(), Mobs_WispCount(), Mobs_SpiderCount());
            I18n_DrawText(log2, sx, gy + 270, 16, (Color){ 255, 140, 160, 255 });

            /* footer */
            int fy = my + 398;
            I18n_DrawText("E - pick a mushroom in the field      1-6 - use a quick slot",
                     mx + 24, fy, 15, (Color){ 170, 170, 195, 255 });
            I18n_DrawText("Shards are currency: they live in the CURRENCY section, not the grid.",
                     mx + 24, fy + 22, 14, (Color){ 170, 200, 190, 255 });
            I18n_DrawText("Click an item, then a slot below to pin it to 1-6.",
                     mx + 24, fy + 40, 14, (Color){ 170, 200, 190, 255 });
            I18n_DrawText("I / ESC - close", mx + 560, fy + 40, 14, (Color){ 140, 140, 165, 255 });
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

    /* v58: four quick slots at the bottom - miniatures of our items;
     * click them (or press 1..4) to use, click in the satchel to assign */
    {
        int hbSize = 56, hbGap = 10;
        int hbW = 6 * hbSize + 5 * hbGap;   /* v65.6: six quick slots */
        int hx = screenWidth / 2 - hbW / 2;
        int hy = screenHeight - 74;
        Texture2D atlasHb = World_GetTerrainTexture();
        bool assignMode = inventoryOpen && screenCursorEnabled && satchelPick >= 0;
        for (int i = 0; i < 6; i++) {
            int cx = hx + i * (hbSize + hbGap);
            hotbarRects[i] = (Rectangle){ (float)cx, (float)hy, (float)hbSize, (float)hbSize };
            hotbarRectsReady = true;
            CosmicCell(cx, hy, hbSize, assignMode);
            int item = Player_HotbarItem(i);
            if (item >= 0) {
                CosmicTileIcon(atlasHb, item, cx + 8, hy + 6, 40);
                int cnt = (item == 27 || item == 73 || item == 74 || item == 75)
                          ? (item == 27 ? Mobs_GetMushrooms() : Mobs_GetMushroomSpecies(item))
                          : (item == 40 ? Player_GetScrollCount() : 0);
                const char *cntS = TextFormat("%d", cnt);
                int tw = I18n_MeasureText(cntS, 15);
                I18n_DrawText(cntS, cx + hbSize - tw - 5 + 1, hy + hbSize - 19 + 1, 15, BLACK);
                I18n_DrawText(cntS, cx + hbSize - tw - 5, hy + hbSize - 19, 15, (Color){ 255, 240, 200, 255 });
            }
            const char *num = TextFormat("%d", i + 1);
            I18n_DrawText(num, cx + 4 + 1, hy + 3 + 1, 13, BLACK);
            I18n_DrawText(num, cx + 4, hy + 3, 13, (Color){ 160, 235, 220, 220 });
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mp = GetMousePosition();
            for (int i = 0; i < 6; i++) {
                if (CheckCollisionPointRec(mp, hotbarRects[i])) {
                    if (assignMode) {
                        Player_HotbarAssign(i, satchelPick);
                        satchelPick = -1;
                        SoundFx_PlayPlace();
                    } else if (!screenCursorEnabled) {
                        Player_HotbarUseSlot(i);
                    }
                }
            }
        }
    }

    /* v58: 'gaze true' reminder ring while the scroll wards the lens */
    if (Player_GetGazeTimeLeft() > 0.0) {
        DrawCircleLines(screenWidth / 2, screenHeight / 2, 15, (Color){ 64, 224, 208, 220 });
        DrawCircleLines(screenWidth / 2, screenHeight / 2, 18, (Color){ 64, 224, 208, 90 });
        const char *gz = TextFormat("gaze true: %ds", (int)(Player_GetGazeTimeLeft() + 0.9));
        I18n_DrawText(gz, screenWidth / 2 - I18n_MeasureText(gz, 18) / 2 + 1, 10 + 1, 18, BLACK);
        I18n_DrawText(gz, screenWidth / 2 - I18n_MeasureText(gz, 18) / 2, 10, 18, (Color){ 120, 255, 235, 255 });
    }

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

    /* v55: readable pause menu - big buttons, clear title */
    int offsetY = screenHeight / 2 - 150;
    int offsetX = screenWidth / 2 - 150;
    DrawPanel((Rectangle){offsetX - 16, offsetY - 16, 332, (float)(52 + 5 * 52 + 14)});

    const char *ptitle = "PAUSED";
    I18n_DrawText(ptitle, offsetX + 165 - I18n_MeasureText(ptitle, 26) / 2 + 1, offsetY + 8 + 1, 26, BLACK);
    I18n_DrawText(ptitle, offsetX + 165 - I18n_MeasureText(ptitle, 26) / 2, offsetY + 8, 26, (Color){ 96, 255, 214, 255 });

    int index = 1;   /* row 0 is the title */

    //Continue Button
    if (CosmicButton((Rectangle) {offsetX , offsetY + (index++ * 52), 300, 42 }, "Continue", true)) {
        Screen_Switch(SCREEN_GAME);
        DisableCursor();
        screenCursorEnabled = false;
        return;
    }

    //Options Button
    if (CosmicButton((Rectangle) {offsetX, offsetY + (index++ * 52), 300, 42 }, "Options", true)) {
        Screen_Switch(SCREEN_OPTIONS);
    }

    if (LocalServer_IsRunning()) {
        if (CosmicButton((Rectangle){offsetX, offsetY + (index++ * 52), 300, 42}, "New World", true)) {
            LocalServer_Stop();
            LocalServer_WipeWorld(false);
            player.flying = false;
            player.blockSelected = 1;
            Bird_Clear();
            MapView_Reset();
            Screen_BeginSingleplayer();
            return;
        }
        if (CosmicButton((Rectangle){offsetX, offsetY + (index++ * 52), 300, 42}, "Regenerate World", true)) {
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
    if (CosmicButton((Rectangle) {offsetX, offsetY + (index++ * 52), 300, 42 }, "Main Menu", true)) {
        if (networkConnectedToServer) {
            Network_Disconnect();
        } else {
            Screen_Switch(SCREEN_LOGIN);
             screenCursorEnabled = false;
            World_Clear();
        }
    }

    //Quit Button
    if (CosmicButton((Rectangle) {offsetX, offsetY + (index++ * 52), 300, 42 }, "Quit", true)) {
        *exitGame = true;
    }
}

void Screen_DrawOptions(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){5, 2, 14, 120});

    /* v55: options at a readable size */
    int offsetY = screenHeight / 2 - 196;
    int offsetX = screenWidth / 2 - 150;
    /* v64.0: the panel is sized to the actual row count (9 rows) - it
     * used to keep an empty 10th row of dead space below the buttons */
    DrawPanel((Rectangle){offsetX - 16, offsetY - 16, 332, (float)(48 + 9 * 50 + 10)});

    const char *otitle = "OPTIONS";
    I18n_DrawText(otitle, offsetX + 150 - I18n_MeasureText(otitle, 24) / 2 + 1, offsetY + 6 + 1, 24, BLACK);
    I18n_DrawText(otitle, offsetX + 150 - I18n_MeasureText(otitle, 24) / 2, offsetY + 6, 24, (Color){ 96, 255, 214, 255 });

    int index = 1;
    offsetY += index * 48;

    /* v63.3: draw distance is a real setting again. Smaller = the world
     * fills much faster on the v62 single-thread loader; the fog follows
     * the value automatically so islands still fade before the edge. */
    const char* drawDistanceTxt = TextFormat("Draw Distance: %d", world.drawDistance);
    if (CosmicButton((Rectangle) {offsetX, offsetY, 300, 42 }, drawDistanceTxt, true)) {
        int dd = world.drawDistance == 18 ? 22
               : (world.drawDistance == 22 ? 26
               : (world.drawDistance == 26 ? 30
               : (world.drawDistance == 30 ? 34 : 18)));
        world.drawDistance = dd;
        gameSettings.drawDistance = dd;
        Settings_Save();
        Network_Send(Packet_CreateSetDrawDistance((unsigned char)dd));
    }

    offsetY += 50;

    //Draw Debug Button
    const char* debugStateTxt = "OFF";
    if (screenShowDebug) debugStateTxt = "ON";
    const char* showDebugTxt = TextFormat("Show Debug: %s", debugStateTxt);
    if (CosmicButton((Rectangle) {offsetX, offsetY, 300, 42 }, showDebugTxt, true)) {
        screenShowDebug = !screenShowDebug;
    }

    offsetY += 50;

    //Draw Max FPS
    const char* maxFPSTxt = TextFormat("Max FPS: %s", maxFPS);
    if (CosmicButton((Rectangle) {offsetX, offsetY, 300, 42 }, maxFPSTxt, true)) {
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

    offsetY += 50;

    const char *fullTxt = TextFormat("Fullscreen: %s  (F11)", gameSettings.fullscreen ? "ON" : "OFF");
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42}, fullTxt, true)) {
        Settings_ToggleFullscreen();
    }

    offsetY += 50;

    const char *resTxt = TextFormat("Resolution: %s", Settings_ResolutionLabel());
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42}, resTxt, true)) {
        Settings_CycleResolution();
    }

    offsetY += 50;

    /* v56: VSync (applied next launch, like resolution) */
    const char *vsTxt = TextFormat("VSync: %s  (next launch)", gameSettings.vsync ? "ON" : "OFF");
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42}, vsTxt, true)) {
        gameSettings.vsync = !gameSettings.vsync;
        Settings_Save();
    }

    offsetY += 50;

    /* v59: dungeon-synth radio */
    const char *muTxt = gameSettings.music ? "Music: ON" : "Music: OFF";
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42}, muTxt, true)) {
        gameSettings.music = !gameSettings.music;
        SoundFx_SetMusicEnabled(gameSettings.music != 0);
        Settings_Save();
    }

    offsetY += 50;

    /* v63.5: auto track switching - off by default, N skips manually */
    const char *atTxt = gameSettings.autoTrack ? "Auto Tracks: ON" : "Auto Tracks: OFF";
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42}, atTxt, true)) {
        gameSettings.autoTrack = !gameSettings.autoTrack;
        Settings_Save();
    }

    offsetY += 50;

    /* v59: language */
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42},
                     TextFormat("Language: %s", I18n_LangLabel(I18n_GetLanguage())), true)) {
        I18n_SetLanguage((I18n_GetLanguage() + 1) % LANG_COUNT);
        gameSettings.language = I18n_GetLanguage();
        Settings_Save();
    }

    offsetY += 50;

    float vol = SoundFx_GetVolume();
    GuiSlider((Rectangle){offsetX, offsetY, 300, 38}, "", "", &vol, 0.0f, 1.0f);
    const char *volTxt = TextFormat("Volume: %i%%", (int)(vol * 100.0f + 0.5f));
    I18n_DrawText(volTxt, offsetX + 150 - I18n_MeasureText(volTxt, 16) / 2 + 1, offsetY + 42 + 1, 16, BLACK);
    I18n_DrawText(volTxt, offsetX + 150 - I18n_MeasureText(volTxt, 16) / 2, offsetY + 42, 16, (Color){ 200, 200, 220, 255 });
    if (fabsf(vol - SoundFx_GetVolume()) > 0.001f) {
        SoundFx_SetVolume(vol);
        gameSettings.volume = (int)(vol * 100.0f + 0.5f);
        Settings_Save();
    }

    offsetY += 74;

    if (CosmicButton((Rectangle) {offsetX, offsetY, 300, 42 }, "Back", true)) {
        Screen_Switch(SCREEN_PAUSE);
    }

}

void Screen_DrawJoining(void) {
    DrawRectangle(0, 0, screenWidth, screenHeight, BLACK);
    I18n_DrawText("Joining Server...", screenWidth / 2 - 80, screenHeight / 2 - 30, 20, WHITE);
}

char nameInput[16] = "Player";
char ipInput[128] = "localhost";
char portInput[5] = "25565";

bool loginEditMode = false;
bool ipEditMode = false;
bool portEditMode = false;

/* v65.34: the whole main menu is rebuilt - flat surfaces, one accent,
 * generous spacing, sectioned right column. No bevelled 90s panels. */
static bool menuThemeReady = false;
static void MenuApplyDarkTheme(void) {
    if (menuThemeReady) return;
    menuThemeReady = true;
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x39424aff);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x1a1f25ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xcfd6dcff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x60d8c4ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x22323aff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xe6fffaFF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0x78e0ccff);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x2a4a44ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xffffffff);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x10141aff);
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
}

static bool FlatMenuButton(Rectangle b, const char *label, bool primary) {
    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, b);
    Color bg, bd, tc;
    if (primary) {
        bg = hover ? (Color){ 34, 92, 78, 235 } : (Color){ 24, 62, 54, 220 };
        bd = hover ? (Color){ 120, 255, 214, 255 } : (Color){ 74, 160, 138, 255 };
        tc = hover ? WHITE : (Color){ 190, 240, 225, 255 };
    } else {
        bg = hover ? (Color){ 38, 45, 53, 225 } : (Color){ 20, 24, 30, 200 };
        bd = hover ? (Color){ 96, 216, 196, 255 } : (Color){ 50, 58, 66, 255 };
        tc = hover ? WHITE : (Color){ 206, 214, 222, 255 };
    }
    DrawRectangleRec(b, bg);
    DrawRectangleLinesEx(b, 1.0f, bd);
    if (primary) DrawRectangle((int)b.x, (int)b.y, 3, (int)b.height, (Color){ 96, 216, 196, 255 });
    float tw = I18n_MeasureText(label, 18);
    I18n_DrawText(label, (int)(b.x + b.width / 2 - tw / 2), (int)(b.y + b.height / 2 - 9), 18, tc);
    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void FlatPanel(Rectangle b, const char *sectionLabel) {
    DrawRectangleRec(b, (Color){ 14, 17, 22, 205 });
    DrawRectangleLinesEx(b, 1.0f, (Color){ 44, 51, 59, 255 });
    DrawRectangle((int)b.x, (int)b.y, 3, (int)b.height, (Color){ 58, 130, 118, 255 });
    if (sectionLabel && sectionLabel[0])
        I18n_DrawText(sectionLabel, (int)b.x + 16, (int)b.y + 10, 14, (Color){ 122, 200, 184, 255 });
}

void Screen_DrawLogin(void) {
    if (IsCursorHidden()) EnableCursor();
    MenuApplyDarkTheme();
    DrawMenuBackground();

    int sw = screenWidth, sh = screenHeight;
    /* left-side scrim so the text always reads over the stars */
    DrawRectangleGradientH(0, 0, (int)(sw * 0.58f), sh,
                           (Color){ 6, 8, 12, 228 }, (Color){ 6, 8, 12, 0 });

    /* ---- left column: identity + primary actions ---- */
    int lx = sw / 16;
    int ty = (int)(sh * 0.11f);
    const char *title = "MIDLESS";
    I18n_DrawText(title, lx + 3, ty + 3, 64, (Color){ 12, 30, 34, 200 });
    I18n_DrawText(title, lx, ty, 64, (Color){ 235, 242, 246, 255 });
    I18n_DrawText("COSMIC EDITION", lx + 4, ty + 78, 16, (Color){ 96, 216, 196, 255 });
    DrawRectangle(lx + 4, ty + 104, 240, 2, (Color){ 96, 216, 196, 220 });
    I18n_DrawText("floating islands - starlit void - the sun is a black hole",
                  lx + 4, ty + 116, 14, (Color){ 150, 162, 176, 220 });

    int by = (int)(sh * 0.46f);
    float bw = 320, bh = 46, gap = 14;
    if (FlatMenuButton((Rectangle){ (float)lx, (float)by, bw, bh }, "SINGLEPLAYER", true)) {
        /* v65.7: the hosted server reads exactly what these boxes say */
        LocalServer_WriteHostConfig(hostPortInput, hostMaxInput, hostNameInput);
        /* v65.32: the chosen parkour map rides into the server session */
        ParkourMapSetActive(gameSettings.parkourMap);
        Screen_BeginSingleplayer();
    }
    if (FlatMenuButton((Rectangle){ (float)lx, by + (bh + gap), bw, bh }, "PARKOUR MAP EDITOR", false))
        MapEdit_Enter();
    if (FlatMenuButton((Rectangle){ (float)lx, by + 2 * (bh + gap), bw, bh }, "OPTIONS", false))
        Screen_Switch(SCREEN_OPTIONS);
    if (FlatMenuButton((Rectangle){ (float)lx, by + 3 * (bh + gap), bw, bh }, "QUIT", false))
        *exitGame = true;

    /* ---- right column: multiplayer / host / parkour map ---- */
    int rx = sw - 440;
    float rw = 380;
    if (rx < lx + 380) rx = lx + 380;   /* never slide under the left column */

    int my = (int)(sh * 0.10f);
    FlatPanel((Rectangle){ (float)rx, (float)my, rw, 176 }, "MULTIPLAYER - join a friend's world");
    if (GuiTextBox((Rectangle){ rx + 16, my + 36, rw - 32, 30 }, nameInput, 16, loginEditMode))
        loginEditMode = !loginEditMode;
    I18n_DrawText("player name", rx + 20, my + 70, 11, (Color){ 120, 130, 142, 230 });
    if (GuiTextBox((Rectangle){ rx + 16, my + 86, rw - 130, 30 }, ipInput, 128, ipEditMode))
        ipEditMode = !ipEditMode;
    if (GuiTextBox((Rectangle){ rx + rw - 106, my + 86, 90, 30 }, portInput, 5, portEditMode))
        portEditMode = !portEditMode;
    I18n_DrawText("address                                    port", rx + 20, my + 120, 11, (Color){ 120, 130, 142, 230 });
    if (FlatMenuButton((Rectangle){ rx + 16, my + 134, rw - 32, 32 }, "CONNECT", true)) {
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

    /* v65.7: HOST SETTINGS - a server you can hand to a friend starts
     * here: port, player cap and name, saved to server.ini beside the
     * game (the dedicated server.exe reads the same file). */
    if (!hostCfgLoaded) {
        hostCfgLoaded = true;
        int p = 25565, m = 8;
        char n[48];
        LocalServer_ReadHostConfig(n, sizeof(n), &p, &m);
        snprintf(hostPortInput, sizeof(hostPortInput), "%d", p);
        snprintf(hostMaxInput, sizeof(hostMaxInput), "%d", m);
        snprintf(hostNameInput, sizeof(hostNameInput), "%s", n);
    }
    int hy = my + 196;
    FlatPanel((Rectangle){ (float)rx, (float)hy, rw, 146 }, "HOST - friends join via your ip:port");
    if (GuiTextBox((Rectangle){ rx + 16, hy + 36, 196, 28 }, hostNameInput,
                   sizeof(hostNameInput), hostNameEdit)) hostNameEdit = !hostNameEdit;
    if (GuiTextBox((Rectangle){ rx + 220, hy + 36, 66, 28 }, hostPortInput,
                   sizeof(hostPortInput), hostPortEdit)) hostPortEdit = !hostPortEdit;
    if (GuiTextBox((Rectangle){ rx + 294, hy + 36, 70, 28 }, hostMaxInput,
                   sizeof(hostMaxInput), hostMaxEdit)) hostMaxEdit = !hostMaxEdit;
    I18n_DrawText("server name                       port      players",
                  rx + 20, hy + 70, 11, (Color){ 120, 130, 142, 230 });
    I18n_DrawText("applied when you press SINGLEPLAYER; saved to server.ini",
                  rx + 20, hy + 92, 12, (Color){ 130, 140, 152, 235 });
    I18n_DrawText("these settings also feed the dedicated server.exe",
                  rx + 20, hy + 112, 12, (Color){ 110, 120, 132, 220 });

    /* v65.32/v65.34: parkour map switcher - the current pick is always
     * visible between the arrows; maps/ rescans once a second */
    {
        static char names[PMAP_MAX_MAPS][PMAP_NAME_LEN];
        static int count = 0;
        static int sel = 0;
        static int refresh = 0;
        static int appliedSel = -1;
        if (refresh++ % 60 == 1 || (count == 0 && refresh == 1)) {
            count = ParkourMapList(names, PMAP_MAX_MAPS);
            sel = 0;
            for (int i = 0; i < count; i++)
                if (gameSettings.parkourMap[0] && !strcmp(names[i], gameSettings.parkourMap))
                    sel = i + 1;
            appliedSel = sel;
        }
        int py = hy + 166;
        FlatPanel((Rectangle){ (float)rx, (float)py, rw, 128 }, "PARKOUR MODE MAP");
        const char *label = (sel == 0) ? "Default Foundry" : names[sel - 1];
        Rectangle prevB = { rx + 16, py + 32, 36, 32 }, nextB = { rx + rw - 52, py + 32, 36, 32 };
        if (FlatMenuButton(prevB, "<", false)) sel = (sel + count) % (count + 1);
        if (FlatMenuButton(nextB, ">", false)) sel = (sel + 1) % (count + 1);
        /* the name plate between the arrows, long names get clipped */
        Rectangle plate = { rx + 60, py + 32, rw - 120, 32 };
        DrawRectangleRec(plate, (Color){ 10, 13, 17, 255 });
        DrawRectangleLinesEx(plate, 1.0f, (Color){ 52, 60, 68, 255 });
        char clipped[PMAP_NAME_LEN + 1];
        snprintf(clipped, sizeof(clipped), "%s", label);
        while (I18n_MeasureText(clipped, 16) > plate.width - 16 && clipped[0])
            clipped[strlen(clipped) - 1] = 0;
        float tw2 = I18n_MeasureText(clipped, 16);
        I18n_DrawText(clipped, (int)(plate.x + plate.width / 2 - tw2 / 2), (int)(plate.y + 7), 16,
                      sel == 0 ? (Color){ 200, 208, 216, 255 } : (Color){ 130, 226, 202, 255 });
        if (appliedSel != sel) {
            appliedSel = sel;
            if (sel == 0) gameSettings.parkourMap[0] = 0;
            else snprintf(gameSettings.parkourMap, sizeof(gameSettings.parkourMap),
                          "%s", names[sel - 1]);
            Settings_Save();
        }
        if (FlatMenuButton((Rectangle){ rx + 16, py + 74, rw - 32, 30 }, "OPEN MAP EDITOR", false))
            MapEdit_Enter();
        I18n_DrawText("maps live in the maps/ folder next to game.exe",
                      rx + 20, py + 108, 11, (Color){ 110, 120, 132, 220 });
    }

    /* ---- footer hints ---- */
    const char *hint = "WASD move - Space jump - Tab fly - M map - T chat - F5 camera";
    I18n_DrawText(hint, sw / 2 - (int)I18n_MeasureText(hint, 14) / 2, sh - 30, 14,
                  (Color){ 140, 152, 166, 200 });
}

/* v63.4: the world-fill loading screen - a grazer strolls along the
 * progress bar munching flowers while the near-field chunks stream in */
static void Screen_DrawWorldFill(float prog, double elapsed) {
    int w = screenWidth, h = screenHeight;
    /* v63.5: show the NEAR-FIELD gate as 0..100% so the bunny actually
     * reaches the end of the bar when loading completes */
    float disp = prog / 0.95f;
    if (disp > 1.0f) disp = 1.0f;
    if (disp < 0.0f) disp = 0.0f;
    /* dusk sky */
    DrawRectangleGradientV(0, 0, w, h, (Color){22, 12, 44, 255}, (Color){8, 4, 18, 255});
    for (int i = 0; i < 90; i++) {
        int sx = (i * 4567 + 911) % w;
        int sy = (i * 2803 + 173) % (h * 2 / 3);
        float tw = 0.55f + 0.45f * sinf((float)elapsed * 2.0f + i * 1.7f);
        DrawPixel(sx, sy, (Color){210, 220, 255, (unsigned char)(150 * tw)});
    }

    const char *title = "MIDLESS";
    I18n_DrawText(title, w / 2 - I18n_MeasureText(title, 56) / 2, h / 5, 56,
                  (Color){255, 226, 130, 255});
    const char *sub = "Идёт загрузка мира...";
    I18n_DrawText(sub, w / 2 - I18n_MeasureText(sub, 20) / 2, h / 5 + 66, 20,
                  (Color){190, 190, 210, 255});

    /* the bar doubles as the ground the grazer walks on.
     * v63.5: barW snapped to whole 32px tiles so the soil never sticks
     * out of the frame; the frame wraps the tiles with an even margin. */
    int barW = w * 62 / 100;
    barW -= barW % 32;
    int barH = 32;
    int barX = w / 2 - barW / 2, barY = h * 74 / 100;
    Texture2D atlas = ClientTextures_Get(1);
    Rectangle soil = { 2 * 16, 0, 16, 16 };          /* tile 2: dirt */
    Rectangle turf = { 3 * 16, 0, 16, 16 };          /* tile 3: turf */
    for (int tx = 0; tx < barW; tx += 32) {
        Rectangle dst = { barX + tx, barY, 32, 32 };
        DrawTexturePro(atlas, soil, dst, (Vector2){0, 0}, 0, (Color){150, 140, 158, 255});
    }
    int fillW = (int)(barW * disp) / 32 * 32;
    for (int tx = 0; tx < fillW; tx += 32) {
        Rectangle dst = { barX + tx, barY, 32, 32 };
        DrawTexturePro(atlas, turf, dst, (Vector2){0, 0}, 0, WHITE);
    }
    DrawRectangle(barX, barY - 4, fillW, 4, (Color){110, 214, 156, 255});
    DrawRectangleLinesEx((Rectangle){(float)barX - 3, (float)barY - 3,
                                     (float)barW + 6, (float)barH + 6}, 2,
                         (Color){70, 70, 95, 255});

    /* flowers along the bar; the grazer eats them as it passes */
    static float fracs[3] = { 0.22f, 0.47f, 0.72f };
    static int eaten = 0;
    static float chewT = 0.0f;
    if (elapsed < 0.05f || disp < 0.02f) { eaten = 0; chewT = 0; }
    for (int i = 0; i < 3; i++) {
        if (eaten & (1 << i)) continue;
        float fx = barX + fracs[i] * barW;
        if (disp >= fracs[i]) {          /* nom - right at the nose */
            eaten |= (1 << i);
            chewT = 0.7f;
            for (int p = 0; p < 10; p++) {
                DrawCircle(fx + GetRandomValue(-18, 18), barY - 34 + GetRandomValue(-12, 6),
                           2, (Color){230, 150, 200, 220});
            }
            continue;
        }
        DrawLineEx((Vector2){fx, barY}, (Vector2){fx, barY - 22}, 3, (Color){80, 140, 100, 255});
        Rectangle head = { 29 % 16 * 16, 29 / 16 * 16, 16, 16 };
        DrawTexturePro(atlas, head, (Rectangle){fx - 13, barY - 46, 26, 26},
                       (Vector2){0, 0}, 0, WHITE);
    }
    if (chewT > 0) chewT -= GetFrameTime();

    /* the grazer itself: NOSE exactly on the progress point, walking
     * right - the next flower is eaten right in front of its face */
    float gx = barX + barW * disp;
    float gy = barY + 2;
    float gait = sinf((float)elapsed * 10.0f);
    for (int leg = 0; leg < 4; leg++) {
        float lo = (leg % 2 == 0) ? gait * 3.0f : -gait * 3.0f;
        float lx = gx - 62 + leg * 13 + lo;
        DrawRectangle((int)lx, (int)gy - 13, 6, 15, (Color){134, 116, 98, 255});
    }
    DrawCircle(gx - 74, gy - 20, 9, (Color){214, 200, 178, 255});   /* tail puff */
    DrawEllipse(gx - 42, gy - 26, 30, 20, (Color){176, 158, 138, 255});
    DrawEllipse(gx - 42, gy - 16, 22, 10, (Color){198, 182, 160, 255});
    float rosette[5][2] = { {-56, -32}, {-44, -38}, {-32, -30}, {-28, -22}, {-50, -20} };
    for (int r = 0; r < 5; r++)
        DrawCircle(gx + rosette[r][0], gy + rosette[r][1], 3.5f, (Color){122, 142, 116, 255});
    float chewBob = (chewT > 0) ? sinf((float)elapsed * 20.0f) * 3.0f : 0.0f;
    DrawCircle(gx - 14, gy - 30 + chewBob, 14, (Color){186, 168, 146, 255});
    DrawCircle(gx - 2, gy - 26 + chewBob, 8, (Color){204, 188, 162, 255});   /* muzzle */
    DrawEllipse(gx - 19, gy - 45 + chewBob, 5, 12, (Color){150, 132, 112, 255});
    DrawEllipse(gx - 9, gy - 47 + chewBob, 5, 13, (Color){150, 132, 112, 255});
    DrawCircle(gx + 1, gy - 27 + chewBob, 2.6f, (Color){24, 20, 26, 255});

    int pct = (int)(disp * 100.0f + 0.5f);
    const char *pctTxt = TextFormat("%d%%", pct);
    I18n_DrawText(pctTxt, w / 2 - I18n_MeasureText(pctTxt, 22) / 2, barY + 44, 22,
                  (Color){230, 230, 240, 255});
    const char *esc = "ESC - отмена";
    I18n_DrawText(esc, w / 2 - I18n_MeasureText(esc, 15) / 2, h - 34, 15,
                  (Color){150, 150, 175, 255});
}

void Screen_DrawLoading(void) {
    /* v63.4: the world-fill gate - hold here until the spawn disc exists */
    if (World_FillGateActive()) {
        float prog = World_FillGateProgress();
        double elapsed = World_FillGateElapsed();
        if (IsKeyPressed(KEY_ESCAPE)) {
            LocalServer_Stop();
            loadingStarted = false;
            loadingFailed = false;
            EnableCursor();
            World_FillGateEnd();
            Screen_Switch(SCREEN_LOGIN);
            return;
        }
        if ((prog >= 0.95f && elapsed > 1.2) || elapsed > 45.0) {
            /* v65.35: the editor TEST warp fires only now - after the
             * fill gate is done, so loading never hangs on it */
            if (ParkourMapTestWarp()) {
                ParkourMapRequestWarp();
                ParkourMapSetTestWarp(false);
            }
            World_FillGateEnd();
            Screen_Switch(SCREEN_GAME);
            DisableCursor();
            screenCursorEnabled = false;
            return;
        }
        Screen_DrawWorldFill(prog, elapsed);
        return;
    }

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
        I18n_DrawText(err, screenWidth / 2 - I18n_MeasureText(err, 20) / 2, screenHeight / 2 - 30, 20, WHITE);
        I18n_DrawText(hint, screenWidth / 2 - I18n_MeasureText(hint, 16) / 2, screenHeight / 2 + 8, 16,
                 (Color){180, 180, 200, 255});
        if (MenuButton((Rectangle){screenWidth / 2 - 80, screenHeight / 2 + 50, 160, 30}, "Back")) {
            loadingStarted = false;
            loadingFailed = false;
            EnableCursor();
            Screen_Switch(SCREEN_LOGIN);
        }
        return;
    }

    I18n_DrawText("Loading World...", screenWidth / 2 - 90, screenHeight / 2, 20, WHITE);
    I18n_DrawText("ESC to cancel", screenWidth / 2 - 70, screenHeight / 2 + 28, 16,
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
    else if (currentScreen == SCREEN_EDITOR)
        MapEdit_Frame();   /* v65.32: editor draws its own 3D + UI */
    else if (currentScreen == SCREEN_OPTIONS)
        Screen_DrawOptions();
}

void Screen_Switch(Screen screen) {
    currentScreen = screen;
}
