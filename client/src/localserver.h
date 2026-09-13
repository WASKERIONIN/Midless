#ifndef MIDLESS_CLIENT_LOCAL_SERVER_H
#define MIDLESS_CLIENT_LOCAL_SERVER_H

#include <stdbool.h>

bool LocalServer_Start(void);
void LocalServer_Stop(void);
bool LocalServer_IsRunning(void);
void LocalServer_WipeWorld(bool keepSeed);

/* v65.7: host info for the UI - the address a friend types to join */
void LocalServer_ReadHostConfig(char *name, int nameLen, int *port, int *maxPlayers);
void LocalServer_WriteHostConfig(const char *portStr, const char *maxStr, const char *name);
const char *LocalServer_GetLocalIp(void);      /* cached at host start */
int LocalServer_GetPort(void);
int LocalServer_GetMaxPlayers(void);
int LocalServer_GetPlayerCount(void);          /* remote peers + the host */
const char *LocalServer_GetName(void);

#endif
