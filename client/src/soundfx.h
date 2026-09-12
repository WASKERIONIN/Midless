#ifndef MIDLESS_CLIENT_SOUNDFX_H
#define MIDLESS_CLIENT_SOUNDFX_H

#include <stdbool.h>

void SoundFx_Init(void);
void SoundFx_Shutdown(void);
void SoundFx_Update(void);
void SoundFx_PlayDig(void);
void SoundFx_PlayPlace(void);
void SoundFx_PlayJump(void);
void SoundFx_PlayTeleport(void);
void SoundFx_PlayClick(void);
void SoundFx_PlayHunterHit(void);
void SoundFx_PlayHunterDie(void);
void SoundFx_PlayPlayerHurt(void);
void SoundFx_PlayWebShoot(void);
void SoundFx_PlayWebAttach(void);
void SoundFx_PlayExplosion(void);
void SoundFx_SetVolume(float volume01);
float SoundFx_GetVolume(void);
void SoundFx_SetMusicEnabled(bool on);   /* v59: dungeon-synth radio */

#endif
