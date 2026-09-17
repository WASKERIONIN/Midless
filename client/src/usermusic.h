#ifndef MIDLESS_USERMUSIC_H
#define MIDLESS_USERMUSIC_H
/* v65.35: the parkour station plays YOUR music. Drop .mp3/.ogg/.wav/.flac
 * files into the music/ folder next to game.exe - when you enter the
 * parkour zone the folder becomes the playlist (the built-in Foundry
 * station stays for an empty folder). N = next track, B = previous. */
#include <stdbool.h>

void UserMusic_Init(void);            /* scan music/, create it if missing */
void UserMusic_Update(void);          /* per frame: stream + auto-advance */
void UserMusic_EnterParkour(void);    /* zone 2 entry: rescan + start */
void UserMusic_LeaveParkour(void);    /* zone 2 exit: stop + unload */
void UserMusic_Next(void);
void UserMusic_Prev(void);
bool UserMusic_Playing(void);         /* user playlist owns the moment */
bool UserMusic_InParkour(void);       /* inside zone 2 (for diagnostics) */
bool UserMusic_Ducking(void);         /* synth station must stay silent */
const char *UserMusic_TrackName(void);
int  UserMusic_Count(void);

#endif
