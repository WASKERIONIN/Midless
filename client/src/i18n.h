#ifndef MIDLESS_CLIENT_I18N_H
#define MIDLESS_CLIENT_I18N_H

#include "raylib.h"

/* v59: localization - the game ships en / ru / zh / ja / ko.
 * Every static string displayed through I18n_DrawText is translated
 * automatically (the English text is the lookup key); formatted strings
 * wrap their format with L(). All rendering uses a real UTF-8 TTF font,
 * so Cyrillic / CJK / Hangul work everywhere including chat. */

enum { LANG_EN = 0, LANG_RU, LANG_ZH, LANG_JA, LANG_KO, LANG_COUNT };

void I18n_Init(void);
void I18n_Shutdown(void);
void I18n_SetLanguage(int lang);
int I18n_GetLanguage(void);
const char *I18n_LangLabel(int lang);     /* human-readable name */

const char *L(const char *englishSource); /* translate, falls back to input */

Font I18n_Font(void);
/* Drop-in replacements for DrawText / MeasureText (UTF-8, auto-translate) */
void I18n_DrawText(const char *text, int x, int y, int size, Color tint);
float I18n_MeasureText(const char *text, int size);
Vector2 I18n_MeasureEx(const char *text, int size);
void I18n_DrawEx(const char *text, Vector2 pos, int size, Color tint);

#endif
