#include "pch.h"
#include "debug.h"
#include <windows.h>
#include <strsafe.h>
#include <time.h>

static FILE* g_pLogFile = nullptr;
static CRITICAL_SECTION g_csLog;

static void EnsureLogFile() {
    if (g_pLogFile != nullptr) return;

    InitializeCriticalSection(&g_csLog);
    EnterCriticalSection(&g_csLog);

    if (g_pLogFile != nullptr) {
        LeaveCriticalSection(&g_csLog);
        return;
    }

    char sModulePath[MAX_PATH];
    char sLogPath[MAX_PATH];
    GetModuleFileNameA(nullptr, sModulePath, MAX_PATH);

    char* pLastSlash = strrchr(sModulePath, '\\');
    if (pLastSlash != nullptr) {
        *pLastSlash = '\0';
    }

    StringCbPrintfA(sLogPath, MAX_PATH, "%s\\kaentake.log", sModulePath);

    errno_t err = fopen_s(&g_pLogFile, sLogPath, "w");
    if (g_pLogFile != nullptr) {
        time_t now = time(nullptr);
        char timeStr[26];
        ctime_s(timeStr, sizeof(timeStr), &now);
        timeStr[strcspn(timeStr, "\n")] = '\0';
        fprintf(g_pLogFile, "[%s] Log started\n", timeStr);
        fflush(g_pLogFile);
    }

    LeaveCriticalSection(&g_csLog);
}

void LogMessage(LogLevel level, const char* sFormat, ...) {
    EnsureLogFile();

    char sBuffer[2048];
    va_list args;
    va_start(args, sFormat);
    vsprintf_s(sBuffer, sizeof(sBuffer), sFormat, args);
    va_end(args);

    time_t now = time(nullptr);
    char timeStr[26];
    ctime_s(timeStr, sizeof(timeStr), &now);
    timeStr[strcspn(timeStr, "\n")] = '\0';

    const char* levelStr;
    switch (level) {
        case LOG_LEVEL_INFO:  levelStr = "INFO";  break;
        case LOG_LEVEL_DEBUG: levelStr = "DEBUG"; break;
        case LOG_LEVEL_ERROR: levelStr = "ERROR"; break;
        default:              levelStr = "UNKNOWN";
    }

    EnterCriticalSection(&g_csLog);
    if (g_pLogFile != nullptr) {
        fprintf(g_pLogFile, "[%s][%s] %s\n", timeStr, levelStr, sBuffer);
        fflush(g_pLogFile);
    }
    LeaveCriticalSection(&g_csLog);

    OutputDebugStringA(sBuffer);
}

void ErrorMessage(const char* sFormat, ...) {
    char sBuffer[1024];
    va_list args;
    va_start(args, sFormat);
    vsprintf_s(sBuffer, sizeof(sBuffer), sFormat, args);
    va_end(args);

    LOG_ERROR("%s", sBuffer);
    MessageBoxA(nullptr, sBuffer, "Error", MB_ICONERROR);
}
