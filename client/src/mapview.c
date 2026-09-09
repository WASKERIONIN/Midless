#include "mapview.h"
#include "raylib.h"
#include "world.h"
#include "player.h"
#include "block.h"
#include "chunk.h"
#include <math.h>
#include <stdio.h>

#define MAP_SIZE 192
#define MAP_RADIUS 48

static bool open;
static RenderTexture2D target;
static bool ready;
static double lastBuild = -1;

static Color ColorForBlock(int id) {
    switch (id) {
        case 0: return (Color){12, 6, 28, 255};
        case 1: return (Color){64, 54, 96, 255};
        case 2: return (Color){70, 50, 92, 255};
        case 3: return (Color){44, 178, 132, 255};
        case 4: return (Color){94, 62, 118, 255};
        case 5: return (Color){40, 160, 230, 255};
        case 6: return (Color){255, 190, 84, 255};
        case 7: return (Color){148, 64, 255, 255};
        case 10: return (Color){70, 46, 96, 255};
        case 11: return (Color){28, 150, 142, 255};
        case 14: return (Color){64, 214, 255, 255};
        case 19: return (Color){40, 30, 66, 255};
        case 20: return (Color){150, 232, 250, 255};
        case 21: return (Color){255, 160, 60, 255};
        case 22: return (Color){255, 220, 120, 255};
        default: return (Color){96, 74, 140, 255};
    }
}

void MapView_Init(void) {
    target = LoadRenderTexture(MAP_SIZE, MAP_SIZE);
    ready = true;
}

void MapView_Shutdown(void) {
    if (ready) UnloadRenderTexture(target);
    ready = false;
    open = false;
}

void MapView_Toggle(void) { open = !open; }
void MapView_Reset(void) { open = false; }
bool MapView_IsOpen(void) { return open; }

void MapView_Update(void) {
    if (!open || !ready) return;
    if (GetTime() - lastBuild < 0.25) return;
    lastBuild = GetTime();

    int px = (int)floorf(player.position.x);
    int pz = (int)floorf(player.position.z);

    BeginTextureMode(target);
    ClearBackground((Color){8, 4, 22, 255});
    for (int z = -MAP_RADIUS; z < MAP_RADIUS; z++) {
        for (int x = -MAP_RADIUS; x < MAP_RADIUS; x++) {
            int wx = px + x;
            int wz = pz + z;
            int top = 0;
            int found = 0;
            for (int y = (int)player.position.y + 32; y >= (int)player.position.y - 48; y--) {
                int id = World_GetBlock((Vector3){(float)wx, (float)y, (float)wz});
                if (id > 0) {
                    top = id;
                    found = 1;
                    break;
                }
            }
            int sx = (x + MAP_RADIUS) * MAP_SIZE / (MAP_RADIUS * 2);
            int sz = (z + MAP_RADIUS) * MAP_SIZE / (MAP_RADIUS * 2);
            DrawPixel(sx, sz, found ? ColorForBlock(top) : (Color){10, 6, 24, 255});
        }
    }
    DrawPixel(MAP_SIZE / 2, MAP_SIZE / 2, WHITE);
    DrawPixel(MAP_SIZE / 2 + 1, MAP_SIZE / 2, RED);
    EndTextureMode();
}

void MapView_Draw(void) {
    if (!open || !ready) return;
    int x = GetScreenWidth() - MAP_SIZE - 16;
    int y = 96;
    DrawRectangle(x - 4, y - 4, MAP_SIZE + 8, MAP_SIZE + 28, (Color){0, 0, 0, 160});
    DrawTextureRec(target.texture, (Rectangle){0, 0, (float)MAP_SIZE, (float)-MAP_SIZE},
                   (Vector2){(float)x, (float)y}, WHITE);
    DrawText(TextFormat("CHUNK %d,%d  M: close",
                        (int)floorf(player.position.x / CHUNK_SIZE_X),
                        (int)floorf(player.position.z / CHUNK_SIZE_Z)),
             x, y + MAP_SIZE + 4, 10, WHITE);
}
