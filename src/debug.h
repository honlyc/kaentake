#pragma once

#include <cstdio>

#define LOG_INFO(FORMAT, ...) LogMessage(LOG_LEVEL_INFO, FORMAT, __VA_ARGS__)
#define LOG_DEBUG(FORMAT, ...) LogMessage(LOG_LEVEL_DEBUG, FORMAT, __VA_ARGS__)
#define LOG_ERROR(FORMAT, ...) LogMessage(LOG_LEVEL_ERROR, FORMAT, __VA_ARGS__)
#define LOG_WARN(FORMAT, ...) LogMessage(LOG_LEVEL_INFO, "[WARN] " FORMAT, __VA_ARGS__)

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_ERROR
} LogLevel;

void LogMessage(LogLevel level, const char* sFormat, ...);
void ErrorMessage(const char* sFormat, ...);