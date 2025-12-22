// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#include "log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static yurei_log_level_t g_level = YUREI_LOG_INFO;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_use_color = false;

// ANSI color codes
#define ANSI_RESET   "\033[0m"
#define ANSI_GRAY    "\033[90m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_RED     "\033[31m"
#define ANSI_BOLD    "\033[1m"

static const char *level_to_string(yurei_log_level_t level) {
    switch (level) {
    case YUREI_LOG_TRACE:
        return "TRACE";
    case YUREI_LOG_DEBUG:
        return "DEBUG";
    case YUREI_LOG_INFO:
        return "INFO";
    case YUREI_LOG_WARN:
        return "WARN";
    case YUREI_LOG_ERROR:
    default:
        return "ERROR";
    }
}

static const char *level_to_color(yurei_log_level_t level) {
    if (!g_use_color) return "";
    switch (level) {
    case YUREI_LOG_TRACE:
        return ANSI_GRAY;
    case YUREI_LOG_DEBUG:
        return ANSI_CYAN;
    case YUREI_LOG_INFO:
        return ANSI_RESET;
    case YUREI_LOG_WARN:
        return ANSI_YELLOW;
    case YUREI_LOG_ERROR:
        return ANSI_RED ANSI_BOLD;
    default:
        return ANSI_RESET;
    }
}

void yurei_log_init(void) {
    const char *color_env = getenv("YUREI_LOG_COLOR");
    const char *level_env = getenv("YUREI_LOG_LEVEL");
    
    // Enable colors if explicitly set or if terminal detected
    g_use_color = (color_env && (strcmp(color_env, "1") == 0 || strcmp(color_env, "true") == 0));
    
    // Parse log level from environment
    if (level_env) {
        if (strcmp(level_env, "TRACE") == 0) g_level = YUREI_LOG_TRACE;
        else if (strcmp(level_env, "DEBUG") == 0) g_level = YUREI_LOG_DEBUG;
        else if (strcmp(level_env, "INFO") == 0) g_level = YUREI_LOG_INFO;
        else if (strcmp(level_env, "WARN") == 0) g_level = YUREI_LOG_WARN;
        else if (strcmp(level_env, "ERROR") == 0) g_level = YUREI_LOG_ERROR;
    }
}

void yurei_log_set_level(yurei_log_level_t level) {
    pthread_mutex_lock(&g_lock);
    g_level = level;
    pthread_mutex_unlock(&g_lock);
}

void yurei_log_set_color(bool enabled) {
    pthread_mutex_lock(&g_lock);
    g_use_color = enabled;
    pthread_mutex_unlock(&g_lock);
}

void yurei_log_message(yurei_log_level_t level, const char *file, int line, const char *fmt, ...) {
    pthread_mutex_lock(&g_lock);
    if (level < g_level) {
        pthread_mutex_unlock(&g_lock);
        return;
    }
    bool use_color = g_use_color;
    pthread_mutex_unlock(&g_lock);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &tm);

    const char *color = level_to_color(level);
    const char *reset = use_color ? ANSI_RESET : "";

    // Microsecond precision timestamp
    fprintf(stderr, "%s%s.%06ldZ [%-5s] %s:%d: ", 
            color,
            timestamp, 
            ts.tv_nsec / 1000L,  // microseconds
            level_to_string(level), 
            file, 
            line);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    
    fprintf(stderr, "%s\n", reset);
}

