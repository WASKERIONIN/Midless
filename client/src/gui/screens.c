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

/* v54: satchel helpers - a real inventory grid, not a text list */
static void Satchel_Cell(int x, int y, int size, bool highlighted) {
    DrawRectangle(x, y, size, size, (Color){ 14, 8, 30, 235 });
    Color border = highlighted ? (Color){ 96, 255, 214, 220 } : (Color){ 94, 231, 255, 80 };
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)size, (float)size }, 1, border);
    DrawRectangleLinesEx((Rectangle){ (float)x - 1, (float)y - 1, (float)size + 2, (float)size + 2 }, 1,
                         (Color){ 200, 60, 255, 40 });
}

static void Satchel_Count(int x, int y, int size, int n) {
    if (n <= 0) return;
    const char *t = TextFormat("%d", n);
    int w = MeasureText(t, 14);
    DrawText(t, x + size - w - 4 + 1, y + size - 17 + 1, 14, BLACK);
    DrawText(t, x + size - w - 4, y + size - 17, 14, (Color){ 255, 240, 200, 255 });
}

static void Satchel_Pips(int cx, int cy, int lvl) {
    for (int i = 0; i < 3; i++) {
        int px = cx - 14 + i * 10;
        if (i < lvl) DrawRectangle(px, cy, 7, 4, (Color){ 96, 255, 214, 255 });
        else DrawRectangleLinesEx((Rectangle){ (float)px, (float)cy, 7, 4 }, 1, (Color){ 120, 140, 170, 160 });
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

    /* v49.1: laser upgrade menu */
    if (upgradeMenuOpen) {
        /* pause/ESC closes it (cursor was taken away) */
        if (!screenCursorEnabled || currentScreen != SCREEN_GAME) {
            upgradeMenuOpen = false;
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) {
                /* player.c's ESC handler disables the cursor; the auto-close
                 * below catches it - nothing else to do here */
            } else {
                int mx = screenWidth / 2 - 170;
                int my = screenHeight / 2 - 196;
                DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 8, 3, 16, 150 });
                DrawPanel((Rectangle){ (float)mx - 16, (float)my - 16, 372, 408 });

                const char *title = "WARP CORE FORGE";
                DrawText(title, mx + 2, my + 2, 24, BLACK);
                DrawText(title, mx, my, 24, (Color){ 96, 255, 214, 255 });
                const char *shardLine = TextFormat("Void shards: %d", Player_GetShards());
                DrawText(shardLine, mx + 2, my + 32, 16, BLACK);
                DrawText(shardLine, mx, my + 30, 16, (Color){ 200, 160, 255, 255 });

                int rl = Player_GetLaserRangeLvl();
                int lv = Player_GetLaserRateLvl();
                int bl = Player_GetBurstLvl();
                int cl = Player_GetCoolLvl();
                /* v49.4 fix: nested TextFormat shares one static buffer and
                 * garbled these lines - snprintf into locals instead */
                char rangeLine[96], rateLine[96], burstLine[96];
                if (rl >= 3) snprintf(rangeLine, sizeof(rangeLine), "LENS   range %d m   MAX", Player_GetLaserRange());
                else snprintf(rangeLine, sizeof(rangeLine), "LENS   range %d m   lvl %d/3", Player_GetLaserRange(), rl);
                if (lv >= 3) snprintf(rateLine, sizeof(rateLine), "COIL   %.2f s   MAX", Player_GetLaserCooldown());
                else snprintf(rateLine, sizeof(rateLine), "COIL   %.2f s   lvl %d/3", Player_GetLaserCooldown(), lv);
                if (bl >= 3) snprintf(burstLine, sizeof(burstLine), "BURST  click = shot, hold = endless   MAX");
                else if (bl == 2) snprintf(burstLine, sizeof(burstLine), "BURST  click = shot, hold = 5-volleys   lvl 2/3");
                else if (bl == 1) snprintf(burstLine, sizeof(burstLine), "BURST  click = shot, hold = 3-volleys   lvl 1/3");
                else snprintf(burstLine, sizeof(burstLine), "BURST  volley fire   not forged");
                char coolLine[96];
                if (cl >= 3) snprintf(coolLine, sizeof(coolLine), "COOL   arctic coil: pauses cool fast   MAX");
                else if (cl > 0) snprintf(coolLine, sizeof(coolLine), "COOL   faster cooling + quicker shots   lvl %d/3", cl);
                else snprintf(coolLine, sizeof(coolLine), "COOL   heat control   not forged");
                DrawText(rangeLine, mx + 2, my + 62, 15, BLACK);
                DrawText(rangeLine, mx, my + 60, 15, WHITE);
                DrawText(rateLine, mx + 2, my + 84, 15, BLACK);
                DrawText(rateLine, mx, my + 82, 15, WHITE);
                DrawText(burstLine, mx + 2, my + 106, 15, BLACK);
                DrawText(burstLine, mx, my + 104, 15, WHITE);
                DrawText(coolLine, mx + 2, my + 128, 15, BLACK);
                DrawText(coolLine, mx, my + 126, 15, WHITE);

                bool full = (rl >= 3 && lv >= 3 && bl >= 3 && cl >= 3);
                /* v52: MAXed tracks render as plain text, not dead buttons */
                if (rl < 3) {
                    if (MenuButton((Rectangle){ (float)mx, (float)my + 152, 340, 36 }, "UPGRADE LENS  (5 shards)")) {
                        Player_BuyLaserUpgrade(0);
                    }
                } else {
                    DrawText("LENS  MAX", mx + 12, my + 162, 18, (Color){ 120, 190, 170, 255 });
                }
                if (lv < 3) {
                    if (MenuButton((Rectangle){ (float)mx, (float)my + 196, 340, 36 }, "UPGRADE COIL  (5 shards)")) {
                        Player_BuyLaserUpgrade(1);
                    }
                } else {
                    DrawText("COIL  MAX", mx + 12, my + 206, 18, (Color){ 120, 190, 170, 255 });
                }
                if (bl < 3) {
                    if (MenuButton((Rectangle){ (float)mx, (float)my + 240, 340, 36 }, "UPGRADE BURST  (5 shards)")) {
                        Player_BuyLaserUpgrade(2);
                    }
                } else {
                    DrawText("BURST  MAX", mx + 12, my + 250, 18, (Color){ 120, 190, 170, 255 });
                }
                if (cl < 3) {
                    if (MenuButton((Rectangle){ (float)mx, (float)my + 284, 340, 36 }, "UPGRADE COOLING  (5 shards)")) {
                        Player_BuyLaserUpgrade(3);
                    }
                } else {
                    DrawText("COOLING  MAX", mx + 12, my + 294, 18, (Color){ 120, 190, 170, 255 });
                }
                if (full) {
                    const char *done = "The laser is fully forged.";
                    DrawText(done, mx + 2, my + 328, 15, BLACK);
                    DrawText(done, mx, my + 326, 15, (Color){ 96, 255, 214, 255 });
                } else if (Player_GetShards() < 5) {
                    const char *need = "Not enough shards - fell hunters, crawlers, wisps, spiders.";
                    DrawText(need, mx + 2, my + 328, 14, BLACK);
                    DrawText(need, mx, my + 326, 14, (Color){ 255, 120, 140, 255 });
                }
                const char *hint = "B / ESC - close";
                DrawText(hint, mx + 2, my + 356, 14, BLACK);
                DrawText(hint, mx, my + 354, 14, (Color){ 170, 170, 190, 255 });
            }
        }
    }

    /* v53/v54: the satchel (I) - a slot-grid inventory */
    if (inventoryOpen) {
        if (!screenCursorEnabled || currentScreen != SCREEN_GAME) {
            inventoryOpen = false;
        } else {
            int mx = screenWidth / 2 - 175;
            int my = screenHeight / 2 - 140;
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 8, 3, 16, 150 });
            DrawPanel((Rectangle){ (float)mx - 16, (float)my - 16, 382, 300 });

            const char *title = "VOID SATCHEL";
            DrawText(title, mx + 2, my + 2, 22, BLACK);
            DrawText(title, mx, my, 22, (Color){ 96, 255, 214, 255 });

            /* ---- row 1: carry items (56px cells) ---- */
            int iy = my + 40;
            int cell = 56;

            /* shards: teal diamond */
            Satchel_Cell(mx, iy, cell, true);
            Vector2 sc = { mx + cell / 2.0f, iy + 22 };
            DrawPoly(sc, 4, 13.0f, 45.0f, (Color){ 96, 231, 214, 255 });
            DrawPolyLinesEx(sc, 4, 13.0f, 45.0f, 1, (Color){ 220, 255, 250, 255 });
            Satchel_Count(mx, iy, cell, Player_GetShards());
            DrawText("SHARDS", mx, iy + cell + 4, 11, (Color){ 150, 150, 180, 255 });

            /* mushrooms: violet cap + pale stem */
            int mx2 = mx + cell + 10;
            Satchel_Cell(mx2, iy, cell, Mobs_GetMushrooms() > 0);
            int hx = mx2 + cell / 2, hy = iy + 20;
            DrawRectangle(hx - 4, hy + 6, 8, 13, (Color){ 226, 210, 250, 255 });
            DrawCircle(hx, hy + 6, 13.0f, (Color){ 138, 52, 224, 255 });
            DrawCircle(hx, hy + 6, 10.0f, (Color){ 190, 100, 255, 255 });
            DrawCircle(hx - 4, hy + 3, 2.0f, (Color){ 255, 208, 120, 255 });
            Satchel_Count(mx2, iy, cell, Mobs_GetMushrooms());
            DrawText("MUSHROOMS", mx2, iy + cell + 4, 11, (Color){ 150, 150, 180, 255 });

            /* three empty slots for the future */
            for (int e = 0; e < 3; e++) {
                Satchel_Cell(mx2 + (e + 1) * (cell + 10), iy, cell, false);
            }
            DrawText("G - eat a mushroom   E - pick one in the field",
                     mx, iy + cell + 18, 13, (Color){ 170, 170, 195, 255 });

            /* ---- row 2: laser upgrade chips (46px cells with pips) ---- */
            int uy = iy + cell + 40;
            int ucell = 46;
            struct { const char *name; int lvl; } chips[4] = {
                { "LENS", Player_GetLaserRangeLvl() },
                { "COIL", Player_GetLaserRateLvl() },
                { "BURST", Player_GetBurstLvl() },
                { "COOL", Player_GetCoolLvl() },
            };
            for (int c = 0; c < 4; c++) {
                int cx = mx + c * (ucell + 10);
                bool maxed = chips[c].lvl >= 3;
                Satchel_Cell(cx, uy, ucell, maxed);
                int ccx = cx + ucell / 2, ccy = uy + 18;
                if (c == 0) {                      /* lens: ring + dot */
                    DrawCircleLines(ccx, ccy, 9, (Color){ 96, 255, 214, 255 });
                    DrawCircle(ccx, ccy, 3, (Color){ 220, 255, 250, 255 });
                } else if (c == 1) {               /* coil: two rings */
                    DrawCircleLines(ccx, ccy, 10, (Color){ 255, 190, 84, 255 });
                    DrawCircleLines(ccx, ccy, 5, (Color){ 255, 230, 150, 255 });
                } else if (c == 2) {               /* burst: three dots */
                    DrawCircle(ccx - 7, ccy + 4, 2.5f, (Color){ 232, 84, 240, 255 });
                    DrawCircle(ccx, ccy - 2, 2.5f, (Color){ 255, 150, 250, 255 });
                    DrawCircle(ccx + 7, ccy + 4, 2.5f, (Color){ 148, 64, 255, 255 });
                } else {                            /* cool: asterisk */
                    for (int a = 0; a < 6; a++) {
                        float an = 3.1416f * a / 3.0f;
                        DrawLineEx((Vector2){ ccx - cosf(an) * 9, ccy - sinf(an) * 9 },
                                   (Vector2){ ccx + cosf(an) * 9, ccy + sinf(an) * 9 },
                                   1.5f, (Color){ 110, 230, 255, 255 });
                    }
                }
                Satchel_Pips(ccx, uy + ucell - 9, chips[c].lvl);
                int tw = MeasureText(chips[c].name, 11);
                DrawText(chips[c].name, ccx - tw / 2, uy + ucell + 4, 11,
                         maxed ? (Color){ 120, 190, 170, 255 } : (Color){ 150, 150, 180, 255 });
            }
            DrawText("Forge the laser at a warp core (B, 5 shards each)",
                     mx, uy + ucell + 18, 13, (Color){ 170, 170, 195, 255 });

            /* ---- footer: trophies + senses ---- */
            int fy = uy + ucell + 38;
            char line[128];
            snprintf(line, sizeof(line), "Hunters felled: %d      Nearby: %d crawlers, %d wisps, %d spiders",
                     Hunter_GetBounty(), Mobs_CrawlerCount(), Mobs_WispCount(), Mobs_SpiderCount());
            DrawText(line, mx + 2, fy + 1, 13, BLACK);
            DrawText(line, mx, fy, 13, (Color){ 255, 170, 130, 255 });

            const char *hint = "I / ESC - close";
            DrawText(hint, mx, fy + 20, 13, (Color){ 150, 150, 175, 255 });
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

    const char* drawDistanceTxt = "Draw Distance: 20 (fixed)";

    //Draw distance: fixed label (v47.1)
    Vector2 sizeText = MeasureTextEx(GetFontDefault(), drawDistanceTxt, 10.0f, 1);
    DrawTextEx(GetFontDefault(), drawDistanceTxt, (Vector2){offsetX + 100 - sizeText.x / 2 + 1, offsetY + 15 - sizeText.y / 2 + 1}, 10.0f, 1, BLACK);
    DrawTextEx(GetFontDefault(), drawDistanceTxt, (Vector2){offsetX + 100 - sizeText.x / 2, offsetY + 15 - sizeText.y / 2}, 10.0f, 1, WHITE);

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
