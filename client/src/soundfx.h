#ifndef MIDLESS_CLIENT_SOUNDFX_H
#define MIDLESS_CLIENT_SOUNDFX_H

void SoundFx_Init(void);
void SoundFx_Shutdown(void);
void SoundFx_Update(void);
void SoundFx_PlayDig(void);
void SoundFx_PlayPlace(void);
void SoundFx_PlayJump(void);
void SoundFx_PlayTeleport(void);
void SoundFx_PlayClick(void);
void SoundFx_SetVolume(float volume01);
float SoundFx_GetVolume(void);

#endif
