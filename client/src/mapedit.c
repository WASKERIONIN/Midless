#include "raylib.h"
#include "mapedit.h"
#include "parkourmap.h"
#include "screens.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "raymath.h"

#include "raygui.h"   /* implementation lives in screens.c */

/* ------------------------------------------------------------------ state */
typedef enum {
    TOOL_PLACE, TOOL_ERASE, TOOL_BOX, TOOL_WALL, TOOL_DIAG,
    TOOL_GATE, TOOL_START, TOOL_FINISH, TOOL_COUNT
} EditTool;

static const char *TOOL_NAMES[TOOL_COUNT] = {
    "PLACE", "ERASE", "BOX CHAIN", "WALL", "WALL 45", "GATE", "START", "FINISH"
};
static const char *TOOL_HINTS[TOOL_COUNT] = {
    "LMB: put one block on the grid plane",
    "LMB: remove the box under the cursor",
    "LMB corner A, LMB corner B: fill the chain as one slab",
    "LMB A, LMB B: wall of Height/Thickness between them",
    "LMB A, LMB B: 45-degree stepped wall",
    "LMB: set the return-gate marker",
    "LMB: set the course start marker",
    "LMB: set the finish marker"
};

static const int PALETTE_IDS[6] = { 81, 82, 83, 80, 20, 19 };
static const char *PALETTE_NAMES = "Concrete;Concrete Base;Path Lamp;Warp Gate;Crystal;Void Rock";
static const Color PALETTE_COLORS[6] = {
    { 150, 150, 152, 255 }, { 92, 92, 98, 255 }, { 255, 196, 120, 255 },
    { 186, 120, 255, 255 }, { 120, 220, 220, 255 }, { 72, 72, 84, 255 }
};

static PMap map;
static EditTool tool = TOOL_WALL;
static int paletteIndex = 0;
static int wallHeight = 8, wallThickness = 1, boxHeight = 1, gridLevel = 0;
static bool showGrid = true;
static float flySpeed = 24.0f;

static Vector3 camPos = { 96, 46, 40 };
static float camYaw = -0.6f, camPitch = -0.5f;
static bool looking = false;

static bool hasA = false;
static int cellA[3] = { 0 };
static int hoverCell[3] = { 0 };
static bool hoverValid = false;

static PMap undoStack[24];
static int undoCount = 0, redoCount = 0;
static PMap redoStack[24];

static int paletteScroll = 0, mapListScroll = 0;
static char mapNames[PMAP_MAX_MAPS][PMAP_NAME_LEN];
static int mapCount = 0, mapListActive = 0;
static char mapListText[PMAP_MAX_MAPS * (PMAP_NAME_LEN + 1)];
static char saveName[PMAP_NAME_LEN] = "my_course";
static bool nameEdit = false;
static double saveBannerUntil = 0.0;
static char statusText[160];
static bool dirty = false;

/* ------------------------------------------------------------------ utils */
static void RefreshMapList(void) {
    mapCount = ParkourMapList(mapNames, PMAP_MAX_MAPS);
    mapListText[0] = 0;
    for (int i = 0; i < mapCount; i++) {
        if (i) strcat(mapListText, ";");
        strcat(mapListText, mapNames[i]);
    }
    if (mapListActive >= mapCount) mapListActive = mapCount ? mapCount - 1 : 0;
}

static void PushUndo(void) {
    if (undoCount < 24) undoStack[undoCount++] = map;
    redoCount = 0;
    dirty = true;
}

static void Undo(void) {
    if (undoCount <= 0) return;
    redoStack[redoCount++] = map;
    map = undoStack[--undoCount];
    dirty = true;
}

static void Redo(void) {
    if (redoCount <= 0) return;
    undoStack[undoCount++] = map;
    map = redoStack[--redoCount];
    dirty = true;
}

static void AddBox(int x, int y, int z, int hx, int hy, int hz, int id) {
    if (map.boxCount >= PMAP_MAX_BOXES) {
        snprintf(statusText, sizeof(statusText), "Box limit (%d) reached", PMAP_MAX_BOXES);
        return;
    }
    if (x - hx < 0) hx = x;
    if (z - hz < 0) hz = z;
    if (x + hx > 191) hx = 191 - x;
    if (z + hz > 191) hz = 191 - z;
    if (y - hy < 0) hy = y;
    if (y + hy > 63) hy = 63 - y;
    PMapBox *b = &map.boxes[map.boxCount++];
    b->x = x; b->y = y; b->z = z;
    b->hx = hx; b->hy = hy; b->hz = hz; b->id = id;
    dirty = true;
}

static void EraseAt(int x, int y, int z) {
    for (int i = map.boxCount - 1; i >= 0; i--) {
        PMapBox *b = &map.boxes[i];
        if (abs(x - b->x) <= b->hx && abs(y - b->y) <= b->hy && abs(z - b->z) <= b->hz) {
            memmove(&map.boxes[i], &map.boxes[i + 1],
                    (size_t)(map.boxCount - i - 1) * sizeof(PMapBox));
            map.boxCount--;
            dirty = true;
            return;
        }
    }
}

static bool GridCellAtMouse(int out[3]) {
    Camera cam = { camPos,
        { camPos.x + cosf(camPitch) * sinf(camYaw),
          camPos.y + sinf(camPitch),
          camPos.z + cosf(camPitch) * cosf(camYaw) },
        { 0, 1, 0 }, 55.0f, 0 };
    Ray ray = GetMouseRay(GetMousePosition(), cam);
    if (fabsf(ray.direction.y) < 1e-5f) return false;
    float t = ((float)gridLevel + 0.5f - ray.position.y) / ray.direction.y;
    if (t <= 0.0f) return false;
    Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    int x = (int)floorf(hit.x), z = (int)floorf(hit.z);
    if (x < 0 || x > 191 || z < 0 || z > 191) return false;
    out[0] = x; out[1] = gridLevel; out[2] = z;
    return true;
}

static void LineBoxes(int ax, int az, int bx, int bz, bool diag45) {
    int dx = bx - ax, dz = bz - az;
    if (diag45) {
        int sx = dx >= 0 ? 1 : -1, sz = dz >= 0 ? 1 : -1;
        int steps = abs(dx) > abs(dz) ? abs(dx) : abs(dz);
        for (int i = 0; i <= steps; i++)
            AddBox(ax + sx * i, gridLevel + (wallHeight - 1) / 2, az + sz * i,
                   (wallThickness - 1) / 2, (wallHeight - 1) / 2,
                   (wallThickness - 1) / 2, PALETTE_IDS[paletteIndex]);
        return;
    }
    if (abs(dx) >= abs(dz)) {
        int cx = (ax + bx) / 2;
        AddBox(cx, gridLevel + (wallHeight - 1) / 2, az,
               abs(dx) / 2, (wallHeight - 1) / 2, (wallThickness - 1) / 2,
               PALETTE_IDS[paletteIndex]);
    } else {
        int cz = (az + bz) / 2;
        AddBox(ax, gridLevel + (wallHeight - 1) / 2, cz,
               (wallThickness - 1) / 2, (wallHeight - 1) / 2, abs(dz) / 2,
               PALETTE_IDS[paletteIndex]);
    }
}

/* ------------------------------------------------------------------ entry */
void MapEdit_Enter(void) {
    memset(&map, 0, sizeof(map));
    snprintf(map.name, sizeof(map.name), "%s", saveName);
    undoCount = redoCount = 0;
    hasA = false;
    dirty = false;
    camPos = (Vector3){ 96, 46, 40 };
    camYaw = -0.6f; camPitch = -0.5f;
    RefreshMapList();
    /* one-time professional dark theme */
    static bool themed = false;
    if (!themed) {
        themed = true;
        GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x3a4148ff);
        GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x23282dff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xcfd8dcff);
        GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x59a9a0ff);
        GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x2c3a3aff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xe0f2f1ff);
        GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0x79c9bfff);
        GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x35504cff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xffffffff);
        GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x14171aff);
        GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, 0x60686eff);
        GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, 24);
        GuiSetStyle(LISTVIEW, LIST_ITEMS_SPACING, 2);
    }
    currentScreen = SCREEN_EDITOR;
}

/* ------------------------------------------------------------------ frame */
void MapEdit_Frame(void) {
    float dt = GetFrameTime();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    /* ---- fly camera ---- */
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) looking = true;
    if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) looking = false;
    if (looking) {
        camYaw += GetMouseDelta().x * 0.0035f;
        camPitch -= GetMouseDelta().y * 0.0035f;
        if (camPitch > 1.4f) camPitch = 1.4f;
        if (camPitch < -1.4f) camPitch = -1.4f;
    }
    Vector3 fwd = { cosf(camPitch) * sinf(camYaw), sinf(camPitch), cosf(camPitch) * cosf(camYaw) };
    /* strafe basis: v65.32 fix - the previous sign had A/D mirrored
     * relative to where the camera actually looks */
    Vector3 right = { -cosf(camYaw), 0, sinf(camYaw) };
    /* while the map-name box has focus, WASD belong to typing, not flying */
    float speed = flySpeed * dt * (IsKeyDown(KEY_LEFT_SHIFT) ? 3.0f : 1.0f);
    if (!nameEdit) {
        if (IsKeyDown(KEY_W)) camPos = Vector3Add(camPos, Vector3Scale(fwd, speed));
        if (IsKeyDown(KEY_S)) camPos = Vector3Subtract(camPos, Vector3Scale(fwd, speed));
        if (IsKeyDown(KEY_D)) camPos = Vector3Add(camPos, Vector3Scale(right, speed));
        if (IsKeyDown(KEY_A)) camPos = Vector3Subtract(camPos, Vector3Scale(right, speed));
        if (IsKeyDown(KEY_E)) camPos.y += speed;
        if (IsKeyDown(KEY_Q)) camPos.y -= speed;
    }
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) { flySpeed += wheel * 4.0f; if (flySpeed < 4) flySpeed = 4; if (flySpeed > 160) flySpeed = 160; }

    hoverValid = GridCellAtMouse(hoverCell);

    /* ---- tool input (only inside the viewport) ---- */
    bool inViewport = GetMousePosition().x > 224 && GetMousePosition().x < sw - 264 &&
                      GetMousePosition().y > 30 && GetMousePosition().y < sh - 26;
    if (inViewport && hoverValid && !looking && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (tool == TOOL_PLACE) {
            PushUndo();
            AddBox(hoverCell[0], gridLevel, hoverCell[2], 0, 0, 0, PALETTE_IDS[paletteIndex]);
        } else if (tool == TOOL_ERASE) {
            PushUndo();
            EraseAt(hoverCell[0], gridLevel, hoverCell[2]);
        } else if (tool == TOOL_BOX || tool == TOOL_WALL || tool == TOOL_DIAG) {
            if (!hasA) {
                hasA = true;
                memcpy(cellA, hoverCell, sizeof(cellA));
            } else {
                PushUndo();
                if (tool == TOOL_BOX) {
                    int x0 = cellA[0] < hoverCell[0] ? cellA[0] : hoverCell[0];
                    int x1 = cellA[0] < hoverCell[0] ? hoverCell[0] : cellA[0];
                    int z0 = cellA[2] < hoverCell[2] ? cellA[2] : hoverCell[2];
                    int z1 = cellA[2] < hoverCell[2] ? hoverCell[2] : cellA[2];
                    AddBox((x0 + x1) / 2, gridLevel + (boxHeight - 1) / 2, (z0 + z1) / 2,
                           (x1 - x0) / 2, (boxHeight - 1) / 2, (z1 - z0) / 2,
                           PALETTE_IDS[paletteIndex]);
                } else {
                    LineBoxes(cellA[0], cellA[2], hoverCell[0], hoverCell[2], tool == TOOL_DIAG);
                }
                hasA = false;
            }
        } else if (tool == TOOL_GATE) {
            PushUndo();
            map.gate[0] = hoverCell[0]; map.gate[1] = gridLevel; map.gate[2] = hoverCell[2];
            map.hasGate = true;
        } else if (tool == TOOL_START) {
            PushUndo();
            map.start[0] = hoverCell[0]; map.start[1] = gridLevel; map.start[2] = hoverCell[2];
            map.hasStart = true;
        } else if (tool == TOOL_FINISH) {
            PushUndo();
            map.finish[0] = hoverCell[0]; map.finish[1] = gridLevel; map.finish[2] = hoverCell[2];
            map.hasFinish = true;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) { hasA = false; nameEdit = false; }
    if (!nameEdit) {
        if (IsKeyPressed(KEY_Z) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) Undo();
        if (IsKeyPressed(KEY_Y) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) Redo();
    }

    /* ---- 3D viewport ---- */
    ClearBackground((Color){ 18, 21, 26, 255 });
    Camera cam = { camPos,
        { camPos.x + fwd.x, camPos.y + fwd.y, camPos.z + fwd.z }, { 0, 1, 0 }, 55.0f, 0 };
    BeginMode3D(cam);
    if (showGrid) {
        for (int i = 0; i <= 192; i += 8) {
            Color gc = (i % 32 == 0) ? (Color){ 70, 84, 92, 255 } : (Color){ 40, 47, 54, 255 };
            DrawLine3D((Vector3){ i, gridLevel, 0 }, (Vector3){ i, gridLevel, 192 }, gc);
            DrawLine3D((Vector3){ 0, gridLevel, i }, (Vector3){ 192, gridLevel, i }, gc);
        }
    }
    for (int i = 0; i < map.boxCount; i++) {
        const PMapBox *b = &map.boxes[i];
        Color c = { 150, 150, 152, 255 };
        for (int p = 0; p < 6; p++)
            if (PALETTE_IDS[p] == b->id) c = PALETTE_COLORS[p];
        Vector3 center = { (float)b->x + 0.5f, (float)b->y + 0.5f, (float)b->z + 0.5f };
        Vector3 size = { (float)(2 * b->hx + 1), (float)(2 * b->hy + 1), (float)(2 * b->hz + 1) };
        DrawCube(center, size.x, size.y, size.z, c);
        DrawCubeWires(center, size.x, size.y, size.z, (Color){ 20, 24, 28, 255 });
    }
    /* markers */
    if (map.hasGate) {
        DrawLine3D((Vector3){ map.gate[0] + 0.5f, 0, map.gate[2] + 0.5f },
                   (Vector3){ map.gate[0] + 0.5f, 40, map.gate[2] + 0.5f }, (Color){ 186, 120, 255, 200 });
    }
    if (map.hasStart) {
        DrawLine3D((Vector3){ map.start[0] + 0.5f, 0, map.start[2] + 0.5f },
                   (Vector3){ map.start[0] + 0.5f, 40, map.start[2] + 0.5f }, (Color){ 120, 255, 160, 200 });
    }
    if (map.hasFinish) {
        DrawLine3D((Vector3){ map.finish[0] + 0.5f, 0, map.finish[2] + 0.5f },
                   (Vector3){ map.finish[0] + 0.5f, 40, map.finish[2] + 0.5f }, (Color){ 255, 210, 90, 200 });
    }
    /* ghost preview */
    if (hasA && hoverValid) {
        Color ghost = { 120, 220, 200, 70 };
        if (tool == TOOL_BOX) {
            int x0 = cellA[0] < hoverCell[0] ? cellA[0] : hoverCell[0];
            int x1 = cellA[0] < hoverCell[0] ? hoverCell[0] : cellA[0];
            int z0 = cellA[2] < hoverCell[2] ? cellA[2] : hoverCell[2];
            int z1 = cellA[2] < hoverCell[2] ? hoverCell[2] : cellA[2];
            Vector3 c = { (x0 + x1 + 1) / 2.0f, gridLevel + boxHeight / 2.0f, (z0 + z1 + 1) / 2.0f };
            DrawCube(c, x1 - x0 + 1, boxHeight, z1 - z0 + 1, ghost);
            DrawCubeWires(c, x1 - x0 + 1, boxHeight, z1 - z0 + 1, (Color){ 160, 255, 230, 160 });
        } else if (tool == TOOL_WALL || tool == TOOL_DIAG) {
            int ax = cellA[0], az = cellA[2], bx = hoverCell[0], bz = hoverCell[2];
            if (tool == TOOL_DIAG) {
                int sx = bx >= ax ? 1 : -1, sz = bz >= az ? 1 : -1;
                int steps = abs(bx - ax) > abs(bz - az) ? abs(bx - ax) : abs(bz - az);
                for (int i = 0; i <= steps; i++)
                    DrawCube((Vector3){ ax + sx * i + 0.5f, gridLevel + wallHeight / 2.0f, az + sz * i + 0.5f },
                             wallThickness, wallHeight, wallThickness, ghost);
            } else if (abs(bx - ax) >= abs(bz - az)) {
                DrawCube((Vector3){ (ax + bx + 1) / 2.0f, gridLevel + wallHeight / 2.0f, az + 0.5f },
                         abs(bx - ax) + 1, wallHeight, wallThickness, ghost);
            } else {
                DrawCube((Vector3){ ax + 0.5f, gridLevel + wallHeight / 2.0f, (az + bz + 1) / 2.0f },
                         wallThickness, wallHeight, abs(bz - az) + 1, ghost);
            }
        }
    }
    if (hoverValid) {
        DrawCubeWires((Vector3){ hoverCell[0] + 0.5f, gridLevel + 0.5f, hoverCell[2] + 0.5f },
                      1.02f, 1.02f, 1.02f, (Color){ 255, 255, 255, 120 });
    }
    EndMode3D();

    /* ---- workbench UI ---- */
    GuiStatusBar((Rectangle){ 0, (float)sh - 24, (float)sw, 24 }, statusText);
    if (hoverValid)
        snprintf(statusText, sizeof(statusText),
                 "cell %d,%d,%d | %s | boxes %d | fly %.0f | %s",
                 hoverCell[0], hoverCell[1], hoverCell[2], TOOL_NAMES[tool], map.boxCount,
                 flySpeed, hasA ? "corner A set - click B (ESC cancels)" : TOOL_HINTS[tool]);
    else
        snprintf(statusText, sizeof(statusText), "%s | boxes %d | fly %.0f",
                 TOOL_HINTS[tool], map.boxCount, flySpeed);

    /* top toolbar: explicit actions, no hidden menus */
    DrawRectangle(0, 0, sw, 30, (Color){ 24, 28, 33, 255 });
    DrawText("MIDLESS PARKOUR MAP EDITOR", 8, 8, 14, (Color){ 140, 200, 190, 255 });
    int tx = 250;
    if (GuiButton((Rectangle){ tx, 4, 54, 22 }, "NEW")) {
        memset(&map, 0, sizeof(map));
        undoCount = redoCount = 0; hasA = false; dirty = false;
        snprintf(statusText, sizeof(statusText), "New empty map");
    }
    if (GuiButton((Rectangle){ tx + 58, 4, 54, 22 }, "UNDO")) Undo();
    if (GuiButton((Rectangle){ tx + 116, 4, 54, 22 }, "REDO")) Redo();
    if (GuiButton((Rectangle){ tx + 174, 4, 54, 22 }, showGrid ? "GRID*" : "GRID")) showGrid = !showGrid;
    if (GuiButton((Rectangle){ tx + 232, 4, 110, 22 }, "TEST IN GAME")) {
        /* save first so the server picks up the very geometry on screen */
        snprintf(map.name, sizeof(map.name), "%s", saveName);
        if (ParkourMapSave(&map)) {
            dirty = false;
            RefreshMapList();
            snprintf(gameSettings.parkourMap, sizeof(gameSettings.parkourMap), "%s", map.name);
            Settings_Save();
            ParkourMapSetActive(map.name);
            Screen_BeginSingleplayer();
        } else {
            snprintf(statusText, sizeof(statusText), "SAVE FAILED - map not tested");
        }
    }
    DrawText(map.name, tx + 356, 8, 14, dirty ? (Color){ 255, 200, 120, 255 } : (Color){ 150, 160, 170, 255 });

    /* left dock: tools + palette */
    GuiPanel((Rectangle){ 4, 34, 216, sh - 62 }, "TOOLS");
    for (int i = 0; i < TOOL_COUNT; i++) {
        Rectangle r = { 12 + (i % 2) * 104, 58 + (i / 2) * 28, 100, 24 };
        bool on = ((int)tool == i);
        if (on) DrawRectangleRec(r, (Color){ 52, 84, 78, 255 });
        if (GuiButton(r, TOOL_NAMES[i])) tool = (EditTool)i;
    }
    /* row pitch = LIST_ITEMS_HEIGHT(24) + SPACING(2); six rows fit the
     * list without a scrollbar, swatches sit clear of the text column */
    GuiGroupBox((Rectangle){ 12, 176, 200, 190 }, "BLOCKS");
    GuiListView((Rectangle){ 16, 198, 168, 158 }, PALETTE_NAMES, &paletteScroll, &paletteIndex);
    for (int p = 0; p < 6; p++)
        DrawRectangle(190, 205 + p * 26, 16, 16, PALETTE_COLORS[p]);
    GuiGroupBox((Rectangle){ 12, 374, 200, 88 }, "MARKERS");
    DrawText(map.hasGate ? "GATE set" : "GATE -", 20, 396, 12, (Color){ 186, 120, 255, 255 });
    DrawText(map.hasStart ? "START set" : "START -", 20, 416, 12, (Color){ 120, 255, 160, 255 });
    DrawText(map.hasFinish ? "FINISH set" : "FINISH -", 20, 436, 12, (Color){ 255, 210, 90, 255 });

    /* right dock: properties + map library */
    GuiPanel((Rectangle){ sw - 260, 34, 256, sh - 62 }, "PROPERTIES / MAPS");
    int px = sw - 252;
    GuiValueBox((Rectangle){ px, 60, 120, 24 }, "Wall H", &wallHeight, 1, 40, false);
    GuiValueBox((Rectangle){ px + 128, 60, 112, 24 }, "Thick", &wallThickness, 1, 7, false);
    GuiValueBox((Rectangle){ px, 92, 120, 24 }, "Box H", &boxHeight, 1, 40, false);
    GuiValueBox((Rectangle){ px + 128, 92, 112, 24 }, "Grid Y", &gridLevel, 0, 63, false);
    GuiGroupBox((Rectangle){ px - 4, 124, 248, 138 }, "MAP LIBRARY");
    GuiListView((Rectangle){ px, 144, 240, 76 }, mapCount ? mapListText : "(no saved maps)", &mapListScroll, &mapListActive);
    if (GuiButton((Rectangle){ px, 226, 76, 22 }, "LOAD")) {
        if (mapCount > 0 && ParkourMapLoad(mapNames[mapListActive], &map)) {
            snprintf(saveName, sizeof(saveName), "%s", mapNames[mapListActive]);
            undoCount = redoCount = 0;
            dirty = false;
            snprintf(statusText, sizeof(statusText), "Loaded map '%s'", map.name);
        }
    }
    if (GuiButton((Rectangle){ px + 82, 226, 76, 22 }, "DELETE")) {
        if (mapCount > 0) {
            ParkourMapDelete(mapNames[mapListActive]);
            RefreshMapList();
        }
    }
    if (GuiButton((Rectangle){ px + 164, 226, 76, 22 }, "SAVE")) {
        snprintf(map.name, sizeof(map.name), "%s", saveName);
        if (ParkourMapSave(&map)) {
            dirty = false;
            RefreshMapList();
            saveBannerUntil = GetTime() + 3.0f;
            snprintf(statusText, sizeof(statusText), "Saved maps/%s.pmap", map.name);
        } else {
            snprintf(statusText, sizeof(statusText), "SAVE FAILED (see log)");
        }
    }
    if (GuiTextBox((Rectangle){ px, 268, 240, 26 }, saveName, sizeof(saveName), nameEdit))
        nameEdit = !nameEdit;
    DrawText("map file name", px, 300, 11, (Color){ 130, 140, 150, 255 });
    if (GuiButton((Rectangle){ px, 320, 240, 26 }, "EXIT TO MENU")) currentScreen = SCREEN_LOGIN;

    /* loud save feedback - the status bar alone was easy to miss */
    if (GetTime() < saveBannerUntil) {
        char msg[128];
        snprintf(msg, sizeof(msg), "SAVED  maps/%s.pmap", map.name);
        int w = (int)MeasureText(msg, 30) + 40;
        DrawRectangle((sw - w) / 2, 70, w, 52, (Color){ 20, 40, 34, 230 });
        DrawRectangleLines((sw - w) / 2, 70, w, 52, (Color){ 120, 255, 190, 255 });
        DrawText(msg, (sw - w) / 2 + 20, 82, 30, (Color){ 160, 255, 210, 255 });
    }

}
