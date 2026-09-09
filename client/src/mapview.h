#ifndef MIDLESS_CLIENT_MAPVIEW_H
#define MIDLESS_CLIENT_MAPVIEW_H

#include <stdbool.h>

void MapView_Init(void);
void MapView_Shutdown(void);
void MapView_Toggle(void);
void MapView_Reset(void);
bool MapView_IsOpen(void);
void MapView_Update(void);
void MapView_Draw(void);

#endif
