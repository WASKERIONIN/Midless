/*
 * Midless: Cosmic Edition - client log sink (v65.9.1). See clientlog.h.
 */
#include "clientlog.h"
#include "raylib.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *logFile;

static void ClientLog_Trace(int logLevel, const char *text, va_list args) {
    (void)logLevel;
    if (!logFile) return;
    char stamp[32];
    time_t now = time(NULL);
    struct tm tmv = {0};
    const struct tm *local = localtime(&now);
    if (local) tmv = *local;
    strftime(stamp, sizeof(stamp), "%H:%M:%S", &tmv);
    fprintf(logFile, "[%s] ", stamp);
    vfprintf(logFile, text, args);
    fputc('\n', logFile);
    fflush(logFile);
}

void ClientLog_Init(void) {
    logFile = fopen("midless_client.log", "w");
    if (!logFile) return;
    SetTraceLogCallback(ClientLog_Trace);
    TraceLog(LOG_INFO, "Midless client log opened (v65.9.1 diagnostics)");
}

void ClientLog_Shutdown(void) {
    if (logFile) {
        fflush(logFile);
        fclose(logFile);
        logFile = NULL;
    }
}
