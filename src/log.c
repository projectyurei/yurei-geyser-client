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

static const char *level_to_string(yurei_log_level_t level) {
    switch (level) {
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

void yurei_log_set_level(yurei_log_level_t level) {
    pthread_mutex_lock(&g_lock);
    g_level = level;
    pthread_mutex_unlock(&g_lock);
}

void yurei_log_message(yurei_log_level_t level, const char *file, int line, const char *fmt, ...) {
    pthread_mutex_lock(&g_lock);
    if (level < g_level) {
        pthread_mutex_unlock(&g_lock);
        return;
    }
    pthread_mutex_unlock(&g_lock);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &tm);

    fprintf(stderr, "%s.%03ldZ [%s] %s:%d: ", timestamp, ts.tv_nsec / 1000000L, level_to_string(level), file, line);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
