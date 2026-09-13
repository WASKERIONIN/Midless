/*
 * Midless: Cosmic Edition - client log sink (v65.9.1).
 * raylib 4.5 has no SetTraceLogFile, so every TraceLog line is mirrored
 * into midless_client.log next to the executable - the support lifeline
 * when something only reproduces on a real Windows box.
 */
#ifndef CLIENTLOG_H
#define CLIENTLOG_H

void ClientLog_Init(void);
void ClientLog_Shutdown(void);

#endif
