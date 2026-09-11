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
#include "../mobs.h"
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
    bool hover = enabled && CheckCollisionPointRec(mp, bounds);
    DrawRectangleRec(bounds, enabled ? (Color){ 24, 12, 48, 235 } : (Color){ 14, 9, 26, 210 });
    DrawRectangleLinesEx(bounds, hover ? 2 : 1,
        !enabled ? (Color){ 90, 92, 120, 150 } :
        hover   ? (Color){ 96, 255, 214, 255 } : (Color){ 94, 231, 255, 110 });
    int fs = 18;
    int tw = MeasureText(label, fs);
    Color tc = !enabled ? (Color){ 125, 125, 150, 255 }
             : hover   ? (Color){ 225, 255, 250, 255 }
                       : (Color){ 170, 235, 225, 255 };
    DrawText(label, (int)(bounds.x + bounds.width / 2.0f - tw / 2.0f),
             (int)(bounds.y + bounds.height / 2.0f - fs / 2.0f), fs, tc);
    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void CosmicCell(int x, int y, int size, bool highlighted) {
    DrawRectangle(x, y, size, size, (Color){ 14, 8, 30, 235 });
    Color border = highlighted ? (Color){ 96, 255, 214, 230 } : (Color){ 94, 231, 255, 85 };
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)size, (float)size }, 1, border);
    DrawRectangleLinesEx((Rectangle){ (float)x - 1, (float)y - 1, (float)size + 2, (float)size + 2 }, 1,
                         (Color){ 200, 60, 255, 45 });
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
        int dashCharges = 2 - player.dashChargesUsed;
        Vector3 padCheck = { player.position.x, player.position.y - 0.1f, player.position.z };
        const char *weaponTag = player.weaponMode ? "[R: LASER]" : "[R: BLADE]";
        const char *moveText;
        if (player.webActive)
            moveText = TextFormat("%s WEB: hold SHIFT to reel   SPACE release   F detach", weaponTag);
        else if (Player_NearWarpCore())
            moveText = TextFormat("%s E - WARP   B - UPGRADE LASER (5 shards)", weaponTag);
        else if (Mobs_GetMushrooms() > 0)
            moveText = TextFormat("%s G - EAT MUSHROOM x%d   I - SATCHEL", weaponTag, Mobs_GetMushrooms());
        else if (World_GetBlock(padCheck) == 21)
            moveText = TextFormat("%s SPACE - LAUNCH from the pad", weaponTag);
        else if (dashCharges > 0)
            moveText = TextFormat("%s F web   SPACE x2 jump   glide   SHIFT dash x%d   I - SATCHEL", weaponTag, dashCharges);
        else
            moveText = TextFormat("%s dash recharges %.1f   I - SATCHEL", weaponTag, dashLeft);
        int mvX = screenWidth / 2 - MeasureText(moveText, 16) / 2;
        Color mvCol = (player.webActive || dashCharges > 0) ? (Color){94, 255, 214, 255} : (Color){120, 150, 190, 255};
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

        /* v48: shard counter with a tiny wireframe diamond */
        int shards = Player_GetShards();
        const char *shardText = TextFormat("VOID SHARDS: %d", shards);
        int sy = by + 34;
        Color shardCol = shards > 0 ? (Color){96, 255, 214, 255} : (Color){120, 120, 140, 220};
        DrawText(shardText, bx + 1, sy + 1, 14, BLACK);
        DrawText(shardText, bx, sy, 14, shardCol);
        Color dEdge = shardCol;
        DrawLine(bx + 128, sy + 3, bx + 132, sy + 7, dEdge);
        DrawLine(bx + 132, sy + 7, bx + 128, sy + 11, dEdge);
        DrawLine(bx + 128, sy + 11, bx + 124, sy + 7, dEdge);
        DrawLine(bx + 124, sy + 7, bx + 128, sy + 3, dEdge);

        /* v49.1: the edge vignette is gone - the tide speaks through the
         * banner, the hunters' red glow and the swelling drone */
        float tideIncoming = Hunter_GetCalmTimeLeft();
        if (Hunter_GetSurgeTimeLeft() > 0.0f) {
            float pulse = 0.75f + 0.25f * sinf(GetTime() * 6.0f);
            const char *tideText = TextFormat("VOID TIDE  %.0f", Hunter_GetSurgeTimeLeft());
            int tx = screenWidth / 2 - MeasureText(tideText, 28) / 2;
            DrawText(tideText, tx + 2, 44, 28, BLACK);
            DrawText(tideText, tx, 42, 28, (Color){255, 90, 120, (unsigned char)(255.0f * pulse)});
        } else if (tideIncoming > 0.0) {
            const char *warnText = TextFormat("THE VOID STIRS - TIDE IN %.0f", tideIncoming);
            int wx = screenWidth / 2 - MeasureText(warnText, 20) / 2;
            DrawText(warnText, wx + 1, 45, 20, BLACK);
            DrawText(warnText, wx, 44, 20, (Color){255, 190, 110, 230});
        }

        /* hurt flash */
        double sinceHurt = GetTime() - player.lastHurtTime;
        if (sinceHurt < 0.3) {
            DrawRectangle(0, 0, screenWidth, screenHeight,
                          (Color){255, 235, 245, (unsigned char)(70.0f * (1.0 - sinceHurt / 0.3))});
        }
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
            DrawText(title, mx + 2, my + 3, 28, BLACK);
            DrawText(title, mx, my, 28, (Color){ 96, 255, 214, 255 });
            DrawLineEx((Vector2){ (float)mx, (float)my + 40 }, (Vector2){ (float)(mx + 720), (float)my + 40 },
                       1, (Color){ 94, 231, 255, 60 });

            /* shard chip, top right */
            CosmicTileIcon(World_GetTerrainTexture(), 35, mx + 620, my + 8, 28);
            const char *shardLine = TextFormat("x %d", Player_GetShards());
            DrawText(shardLine, mx + 654, my + 14, 20, (Color){ 200, 160, 255, 255 });

            int rl = Player_GetLaserRangeLvl();
            int lv = Player_GetLaserRateLvl();
            int bl = Player_GetBurstLvl();
            int cl = Player_GetCoolLvl();
            struct {
                const char *name; const char *effect; int lvl; int kind;
            } cards[4] = {
                { "LENS",  "+6 m laser range",            rl, 0 },
                { "COIL",  "faster shots",                lv, 1 },
                { "BURST", "hold = volleys, endless at max", bl, 2 },
                { "COOL",  "cooler coil, quicker shots",  cl, 3 },
            };

            int y0 = my + 56;
            for (int c = 0; c < 4; c++) {
                int cy = y0 + c * 94;
                bool maxed = cards[c].lvl >= 3;

                /* track socket cells */
                for (int p = 0; p < 3; p++) {
                    int px = mx + 430 + p * 40;
                    if (p < cards[c].lvl) {
                        DrawRectangle(px, cy + 22, 34, 18, (Color){ 30, 90, 80, 235 });
                        DrawRectangle(px + 2, cy + 24, 30, 14, (Color){ 96, 255, 214, 255 });
                    } else {
                        DrawRectangle(px, cy + 22, 34, 18, (Color){ 14, 8, 30, 235 });
                    }
                    DrawRectangleLinesEx((Rectangle){ (float)px, (float)cy + 22, 34, 18 }, 1,
                                         p < cards[c].lvl ? (Color){ 220, 255, 250, 180 }
                                                          : (Color){ 94, 231, 255, 70 });
                }

                /* icon socket */
                CosmicCell(mx + 24, cy + 6, 60, maxed);
                int icx = mx + 54, icy = cy + 36;
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
                } else {                            /* cool */
                    for (int a = 0; a < 6; a++) {
                        float an = 3.1416f * a / 3.0f;
                        DrawLineEx((Vector2){ icx - cosf(an) * 16, icy - sinf(an) * 16 },
                                   (Vector2){ icx + cosf(an) * 16, icy + sinf(an) * 16 },
                                   2.0f, (Color){ 110, 230, 255, 255 });
                    }
                    DrawCircle(icx, icy, 4, (Color){ 220, 250, 255, 255 });
                }

                /* name + effect */
                DrawText(cards[c].name, mx + 104, cy + 10, 22, maxed ? (Color){ 120, 190, 170, 255 }
                                                                     : (Color){ 225, 245, 255, 255 });
                DrawText(cards[c].effect, mx + 104, cy + 38, 14, (Color){ 165, 165, 190, 255 });

                /* buy button */
                if (maxed) {
                    CosmicButton((Rectangle){ (float)(mx + 576), (float)(cy + 10), 124, 42 }, "MAX", false);
                } else {
                    char btxt[48];
                    snprintf(btxt, sizeof(btxt), "FORGE  5");
                    if (CosmicButton((Rectangle){ (float)(mx + 576), (float)(cy + 10), 124, 42 }, btxt,
                                     Player_GetShards() >= 5)) {
                        Player_BuyLaserUpgrade(cards[c].kind);
                    }
                }
                if (c < 3)
                    DrawLineEx((Vector2){ (float)(mx + 24), (float)(cy + 86) },
                               (Vector2){ (float)(mx + 700), (float)(cy + 86) }, 1, (Color){ 94, 231, 255, 30 });
            }

            const char *hint = "B / ESC - close      falling or dying burns out one upgrade";
            DrawText(hint, mx, my + 442, 14, (Color){ 150, 150, 175, 255 });
        }
    }

    /* v55: the satchel - item grid on the left, details on the right */
    if (inventoryOpen) {
        if (!screenCursorEnabled || currentScreen != SCREEN_GAME) {
            inventoryOpen = false;
        } else {
            int mx = screenWidth / 2 - 352;
            int my = screenHeight / 2 - 186;
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 8, 3, 16, 165 });
            DrawPanel((Rectangle){ (float)mx - 16, (float)my - 16, 736, 404 });

            const char *title = "VOID SATCHEL";
            DrawText(title, mx + 2, my + 3, 26, BLACK);
            DrawText(title, mx, my, 26, (Color){ 96, 255, 214, 255 });

            Texture2D atlas = World_GetTerrainTexture();

            /* ---- item grid (left) ---- */
            int gx = mx + 24, gy = my + 52;
            int cell = 72, gap = 12;
            struct { int tile; int count; } items[8] = {
                { 35, Player_GetShards() },
                { 27, Mobs_GetMushrooms() },
                { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 }, { -1, 0 },
            };
            for (int i = 0; i < 8; i++) {
                int cx = gx + (i % 4) * (cell + gap);
                int cy = gy + (i / 4) * (cell + gap);
                CosmicCell(cx, cy, cell, items[i].tile >= 0);
                if (items[i].tile >= 0) {
                    CosmicTileIcon(atlas, items[i].tile, cx + 10, cy + 8, 52);
                    const char *cnt = TextFormat("%d", items[i].count);
                    int w = MeasureText(cnt, 18);
                    DrawText(cnt, cx + cell - w - 6 + 1, cy + cell - 22 + 1, 18, BLACK);
                    DrawText(cnt, cx + cell - w - 6, cy + cell - 22, 18, (Color){ 255, 240, 200, 255 });
                }
            }
            DrawText("carried", gx + 2, gy + 2 * cell + gap + 8, 14, (Color){ 130, 130, 160, 255 });

            /* ---- details (right side) ---- */
            int sx = gx + 4 * (cell + gap) + 26;
            DrawLineEx((Vector2){ (float)(sx - 14), (float)gy - 6 },
                       (Vector2){ (float)(sx - 14), (float)gy + 2 * cell + gap + 2 }, 1,
                       (Color){ 94, 231, 255, 45 });

            DrawText("LASER FORGE", sx, gy - 4, 15, (Color){ 120, 190, 175, 255 });
            struct { const char *name; int lvl; } ups[4] = {
                { "LENS", Player_GetLaserRangeLvl() },
                { "COIL", Player_GetLaserRateLvl() },
                { "BURST", Player_GetBurstLvl() },
                { "COOL", Player_GetCoolLvl() },
            };
            for (int u = 0; u < 4; u++) {
                int uy = gy + 20 + u * 34;
                DrawText(ups[u].name, sx, uy + 2, 17, ups[u].lvl >= 3 ? (Color){ 120, 190, 170, 255 }
                                                                      : (Color){ 225, 245, 255, 255 });
                CosmicPips(sx + 90, uy + 6, ups[u].lvl);
                const char *lv = TextFormat("%d/3", ups[u].lvl);
                DrawText(lv, sx + 172, uy + 2, 15, (Color){ 150, 150, 180, 255 });
            }
            DrawText("forge at warp cores - B, 5 shards each", sx, gy + 162, 14, (Color){ 140, 140, 165, 255 });

            DrawLineEx((Vector2){ (float)sx, (float)gy + 186 }, (Vector2){ (float)(sx + 270), (float)gy + 186 }, 1,
                       (Color){ 94, 231, 255, 45 });
            DrawText("FIELD LOG", sx, gy + 196, 15, (Color){ 120, 190, 175, 255 });
            const char *log1 = TextFormat("Hunters felled: %d", Hunter_GetBounty());
            DrawText(log1, sx, gy + 218, 16, (Color){ 255, 200, 120, 255 });
            const char *log2 = TextFormat("Nearby: %d crawlers, %d wisps, %d spiders",
                                          Mobs_CrawlerCount(), Mobs_WispCount(), Mobs_SpiderCount());
            DrawText(log2, sx, gy + 242, 16, (Color){ 255, 140, 160, 255 });

            /* footer */
            int fy = my + 352;
            DrawText("G - eat a mushroom (+3 HP)      E - pick one in the field",
                     mx + 24, fy, 15, (Color){ 170, 170, 195, 255 });
            DrawText("I / ESC - close", mx + 24, fy + 22, 14, (Color){ 140, 140, 165, 255 });
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

    /* v55: readable pause menu - big buttons, clear title */
    int offsetY = screenHeight / 2 - 150;
    int offsetX = screenWidth / 2 - 150;
    DrawPanel((Rectangle){offsetX - 16, offsetY - 16, 332, (float)(52 + 5 * 52 + 14)});

    const char *ptitle = "PAUSED";
    DrawText(ptitle, offsetX + 165 - MeasureText(ptitle, 26) / 2 + 1, offsetY + 8 + 1, 26, BLACK);
    DrawText(ptitle, offsetX + 165 - MeasureText(ptitle, 26) / 2, offsetY + 8, 26, (Color){ 96, 255, 214, 255 });

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
    DrawPanel((Rectangle){offsetX - 16, offsetY - 16, 332, (float)(48 + 10 * 50 + 10)});

    const char *otitle = "OPTIONS";
    DrawText(otitle, offsetX + 150 - MeasureText(otitle, 24) / 2 + 1, offsetY + 6 + 1, 24, BLACK);
    DrawText(otitle, offsetX + 150 - MeasureText(otitle, 24) / 2, offsetY + 6, 24, (Color){ 96, 255, 214, 255 });

    int index = 1;
    offsetY += index * 48;

    const char* drawDistanceTxt = "Draw Distance: 20 (fixed)";
    DrawText(drawDistanceTxt, offsetX + 150 - MeasureText(drawDistanceTxt, 16) / 2 + 1, offsetY + 13 + 1, 16, BLACK);
    DrawText(drawDistanceTxt, offsetX + 150 - MeasureText(drawDistanceTxt, 16) / 2, offsetY + 13, 16, (Color){ 200, 200, 220, 255 });

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

    /* v56: FXAA - the practical AA (MSAA is lost inside the postfx pass) */
    const char *fxaaTxt = TextFormat("Smoothing (FXAA): %s", gameSettings.fxaa ? "ON" : "OFF");
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42}, fxaaTxt, true)) {
        gameSettings.fxaa = !gameSettings.fxaa;
        Settings_Save();
    }

    offsetY += 50;

    /* v56: anisotropic x8 on the terrain atlas */
    const char *anTxt = TextFormat("Anisotropic x8: %s", gameSettings.aniso ? "ON" : "OFF");
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42}, anTxt, true)) {
        gameSettings.aniso = !gameSettings.aniso;
        Settings_Save();
    }

    offsetY += 50;

    /* v56: VSync (applied next launch, like resolution) */
    const char *vsTxt = TextFormat("VSync: %s  (next launch)", gameSettings.vsync ? "ON" : "OFF");
    if (CosmicButton((Rectangle){offsetX, offsetY, 300, 42}, vsTxt, true)) {
        gameSettings.vsync = !gameSettings.vsync;
        Settings_Save();
    }

    offsetY += 50;

    float vol = SoundFx_GetVolume();
    GuiSlider((Rectangle){offsetX, offsetY, 300, 38}, "", "", &vol, 0.0f, 1.0f);
    const char *volTxt = TextFormat("Volume: %i%%", (int)(vol * 100.0f + 0.5f));
    DrawText(volTxt, offsetX + 150 - MeasureText(volTxt, 16) / 2 + 1, offsetY + 42 + 1, 16, BLACK);
    DrawText(volTxt, offsetX + 150 - MeasureText(volTxt, 16) / 2, offsetY + 42, 16, (Color){ 200, 200, 220, 255 });
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
