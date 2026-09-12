/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "i18n.h"
#include "raygui.h"
#include "chat.h"
#include "screens.h"
#include "player.h"
#include "world.h"
#include "block.h"
#include "networkhandler.h"
#include "packet.h"
#include "worldtime.h"

char *chatLines[64];
int currentLine = 0;

#define HISTORY_MAX 32
static char history[HISTORY_MAX][64];
static int historyCount;
static int historyView = -1;

void Chat_AddOwnedLine(char *line) {
    if (chatLines[currentLine]) MemFree(chatLines[currentLine]);
    chatLines[currentLine++] = line;
    if (currentLine >= 64) currentLine = 0;
}

void Chat_AddLine(const char *text) {
    text = Tr(text);   /* v59: chat speaks the player's language */
    if (!text) return;
    int len = TextLength(text);
    char *copy = MemAlloc((size_t)len + 1);
    if (!copy) return;
    memcpy(copy, text, (size_t)len + 1);
    Chat_AddOwnedLine(copy);
}

void Chat_Shutdown(void) {
    for (int i = 0; i < 64; i++) {
        MemFree(chatLines[i]);
        chatLines[i] = NULL;
    }
    currentLine = 0;
}

char chatInput[64] = "";
bool chatEditMode = false;
bool chatOpen = false;

static void Remember(const char *line) {
    if (!line[0]) return;
    if (historyCount > 0 && strcmp(history[historyCount - 1], line) == 0) return;
    if (historyCount < HISTORY_MAX) {
        strncpy(history[historyCount++], line, 63);
    } else {
        memmove(history[0], history[1], sizeof(history) - 64);
        strncpy(history[HISTORY_MAX - 1], line, 63);
    }
}

static bool HandleCommand(const char *message) {
    if (message[0] != '/') return false;
    if (strcmp(message, "/help") == 0) {
        Chat_AddLine("Commands: /where /tp x y z /spawn /fly");
        Chat_AddLine("          /time day|night /giveme id");
        return true;
    }
    if (strcmp(message, "/fly") == 0) {
        player.flying = !player.flying;
        player.velocity.y = 0;
        Chat_AddLine(player.flying ? "Fly mode ON (Tab to walk)" : "Fly mode OFF");
        return true;
    }
    if (strcmp(message, "/spawn") == 0) {
        Player_Teleport((Vector3){ 8.0f, 78.0f, 8.0f });
        Chat_AddLine("Returned to the starter island.");
        return true;
    }
    if (strcmp(message, "/where") == 0) {
        Chat_AddLine(TextFormat("X: %i Y: %i Z: %i", (int)player.position.x, (int)player.position.y,
                                (int)player.position.z));
        return true;
    }
    float x, y, z;
    if (sscanf(message, "/tp %f %f %f", &x, &y, &z) == 3) {
        Player_Teleport((Vector3){x, y, z});
        Chat_AddLine(TextFormat("Teleported to %.1f %.1f %.1f", x, y, z));
        return true;
    }
    if (strncmp(message, "/tp ", 4) == 0) {
        Chat_AddLine("Usage: /tp x y z");
        return true;
    }
    if (strcmp(message, "/time day") == 0) {
        world.time = 0;
        Chat_AddLine("Time set to day");
        return true;
    }
    if (strcmp(message, "/time night") == 0) {
        world.time = WORLD_DAY_LENGTH_SECONDS * 0.5f;
        Chat_AddLine("Time set to night");
        return true;
    }
    if (strncmp(message, "/time", 5) == 0) {
        Chat_AddLine("Usage: /time day|night");
        return true;
    }
    int id = 0;
    if (sscanf(message, "/giveme %d", &id) == 1) {
        if (id < 1 || id > 255 || !Block_IsSelectable(id)) {
            Chat_AddLine("Usage: /giveme 1..255 (selectable)");
            return true;
        }
        player.blockSelected = id;
        Chat_AddLine(TextFormat("Selected block %d", id));
        return true;
    }
    if (strncmp(message, "/giveme", 7) == 0) {
        Chat_AddLine("Usage: /giveme 1..255 (selectable)");
        return true;
    }
    Chat_AddLine("Unknown command. Type /help");
    return true;
}

void Chat_Draw(Vector2 offset, Color uiColor) {

    int chatWidth = 352;
    int fontSize = 10;

    if (chatEditMode) DrawRectangle(offset.x, offset.y - 184 + 46, chatWidth, 184, uiColor);

    Color textColor = WHITE;
    Color shadowColor = BLACK;
    if (!chatEditMode) {
        textColor.a = 150;
        shadowColor.a = 150;
    }

    int lineAdded = 0;
    int index = currentLine == 0 ? 63 : currentLine - 1;
    while (lineAdded < 13) {
        if (chatLines[index]) {
            int textLength = TextLength(chatLines[index]);
            int startPos = 0;
            char drawLines[8][132] = {0};
            int drawLinesCnt = 0;
            for (int i = 0; i < textLength && drawLinesCnt < 8; i++) {
                const char *sub = TextSubtext(chatLines[index], startPos, i - startPos + 1);
                int textWidth = (int)I18n_MeasureText(sub, fontSize);
                if (textWidth >= chatWidth - fontSize - 4 || i == textLength - 1) {
                    TextCopy(drawLines[drawLinesCnt], sub);
                    drawLinesCnt++;
                    startPos = i + 1;
                }
            }
            for (int i = drawLinesCnt - 1; i >= 0; i--) {
                if (!drawLines[i]) continue;
                I18n_DrawText(drawLines[i], offset.x + 4 + 1, (int)(offset.y - lineAdded * fontSize + 1), fontSize,
                         shadowColor);
                I18n_DrawText(drawLines[i], offset.x + 4, (int)(offset.y - lineAdded * fontSize), fontSize, textColor);
                lineAdded++;
            }
        }

        index--;
        if (index < 0) index = 63;
        if (index == currentLine) break;
    }

    if (chatEditMode) {
        /* v59: custom UTF-8 input - GuiTextBox's default font had no
         * Cyrillic; GetCharPressed delivers proper codepoints */
        int cp = GetCharPressed();
        while (cp > 0) {
            if (cp >= 32) {
                int len = 0;
                const char *enc = CodepointToUTF8(cp, &len);
                if (enc && len > 0) {
                    int cur = (int)strlen(chatInput);
                    if (cur + len < 60) { memcpy(chatInput + cur, enc, len); chatInput[cur + len] = 0; }
                }
            }
            cp = GetCharPressed();
        }
        int back = GetKeyPressed();
        if (back == KEY_BACKSPACE) {
            int cur = (int)strlen(chatInput);
            while (cur > 0 && ((unsigned char)chatInput[cur - 1] & 0xC0) == 0x80) cur--;
            if (cur > 0) cur--;
            chatInput[cur] = 0;
        }
        if (IsKeyPressed(KEY_UP) && historyCount > 0) {
            if (historyView < 0) historyView = historyCount - 1;
            else if (historyView > 0) historyView--;
            strncpy(chatInput, history[historyView], 63);
        }
        if (IsKeyPressed(KEY_DOWN) && historyCount > 0 && historyView >= 0) {
            historyView++;
            if (historyView >= historyCount) {
                historyView = -1;
                chatInput[0] = 0;
            } else {
                strncpy(chatInput, history[historyView], 63);
            }
        }
        /* v59: the typed line itself, in the UTF-8 font, with a caret */
        {
            int fs = 18;
            Rectangle box = { offset.x, offset.y + 22, chatWidth, 26 };
            DrawRectangleRec(box, (Color){ 6, 3, 14, 200 });
            DrawRectangleLinesEx(box, 1, (Color){ 94, 231, 255, 90 });
            char shown[136];
            snprintf(shown, sizeof(shown), "> %s", chatInput);
            I18n_DrawText(shown, (int)box.x + 6, (int)box.y + 4, fs, (Color){ 235, 245, 255, 255 });
            if (((int)(GetTime() * 2.5)) % 2 == 0) {
                float w = I18n_MeasureText(shown, fs);
                DrawRectangle((int)(box.x + 8 + w), (int)box.y + 4, 2, fs, (Color){ 120, 255, 230, 220 });
            }
        }
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (chatOpen) {
            Remember(chatInput);
            historyView = -1;
            if (chatInput[0] == '/') {
                HandleCommand(chatInput);
                /* Keep /tp and /time on the server so multiplayer stays in sync. */
                if (networkConnectedToServer &&
                    (strncmp(chatInput, "/tp", 3) == 0 || strncmp(chatInput, "/time", 5) == 0)) {
                    Network_Send(Packet_CreateMessage(chatInput));
                }
            } else if (chatInput[0]) {
                char *message = MemAlloc(64);
                for (int i = 0; i < 64; i++) {
                    message[i] = chatInput[i];
                    chatInput[i] = '\0';
                }
                if (networkConnectedToServer) {
                    Network_Send(Packet_CreateMessage(message));
                    MemFree(message);
                } else {
                    Chat_AddOwnedLine(message);
                }
            }
            for (int i = 0; i < 64; i++) chatInput[i] = 0;
            DisableCursor();
            chatOpen = false;
            screenCursorEnabled = false;
        }
    }

    chatEditMode = chatOpen;
}

void Chat_AppendOwnedLine(char *text) {
    int index = currentLine == 0 ? 63 : currentLine - 1;
    if (chatLines[index] == NULL) {
        Chat_AddOwnedLine(text);
        return;
    }

    int lineLength = TextLength(chatLines[index]);
    int textLength = TextLength(text);
    char *combined = MemAlloc(lineLength + textLength + 1);
    if (combined == NULL) {
        MemFree(text);
        return;
    }
    memcpy(combined, chatLines[index], lineLength);
    memcpy(combined + lineLength, text, textLength + 1);
    MemFree(chatLines[index]);
    MemFree(text);
    chatLines[index] = combined;
}
