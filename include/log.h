// Project Yurei - High-performance Solana data engine
// Copyright 2025 Project Yurei. All rights reserved.
// https://x.com/yureiai

#ifndef YUREI_LOG_H
#define YUREI_LOG_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YUREI_LOG_TRACE = 0,
    YUREI_LOG_DEBUG,
    YUREI_LOG_INFO,
    YUREI_LOG_WARN,
    YUREI_LOG_ERROR
} yurei_log_level_t;

// Initialize logging from environment (YUREI_LOG_LEVEL, YUREI_LOG_COLOR)
void yurei_log_init(void);
void yurei_log_set_level(yurei_log_level_t level);
void yurei_log_set_color(bool enabled);
void yurei_log_message(yurei_log_level_t level, const char *file, int line, const char *fmt, ...);

#define YUREI_LOG(level, ...) \
    yurei_log_message(level, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_TRACE(...) YUREI_LOG(YUREI_LOG_TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) YUREI_LOG(YUREI_LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) YUREI_LOG(YUREI_LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...) YUREI_LOG(YUREI_LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...) YUREI_LOG(YUREI_LOG_ERROR, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif

