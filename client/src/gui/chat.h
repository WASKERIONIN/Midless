/**
 * Copyright (c) 2021-2022 Sirvoid
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef MIDLESS_CLIENT_CHAT_H
#define MIDLESS_CLIENT_CHAT_H

extern bool chatOpen;

void Chat_AddOwnedLine(char *line);
void Chat_AddLine(const char *text);
void Chat_AppendOwnedLine(char *text);
void Chat_Draw(Vector2 offset, Color uiColor);
void Chat_Shutdown(void);

#endif
