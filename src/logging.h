#pragma once
#include <Arduino.h>

/* ===== LOG LEVELS ===== */

#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4


/* ========================================================= */
/* ===== LOG MACROS ======================================== */
/* ========================================================= */

/* ===== ERROR ===== */
#if defined(LOG_LEVEL) && (LOG_LEVEL >= LOG_LEVEL_ERROR)
  #define LOG_ERROR(fmt, ...) Serial.printf("[ERR] " fmt "\n", ##__VA_ARGS__)
  #define LOG_ERROR_INLINE(fmt, ...) Serial.printf("\r[ERR] " fmt, ##__VA_ARGS__)
#else
  #define LOG_ERROR(...)
  #define LOG_ERROR_INLINE(...)
#endif

/* ===== WARN ===== */
#if defined(LOG_LEVEL) && (LOG_LEVEL >= LOG_LEVEL_WARN)
  #define LOG_WARN(fmt, ...) Serial.printf("[WRN] " fmt "\n", ##__VA_ARGS__)
  #define LOG_WARN_INLINE(fmt, ...) Serial.printf("\r[WRN] " fmt, ##__VA_ARGS__)
#else
  #define LOG_WARN(...)
  #define LOG_WARN_INLINE(...)
#endif

/* ===== INFO ===== */
#if defined(LOG_LEVEL) && (LOG_LEVEL >= LOG_LEVEL_INFO)
  #define LOG_INFO(fmt, ...) Serial.printf("[INF] " fmt "\n", ##__VA_ARGS__)
  #define LOG_INFO_INLINE(fmt, ...) Serial.printf("\r[INF] " fmt, ##__VA_ARGS__)
#else
  #define LOG_INFO(...)
  #define LOG_INFO_INLINE(...)
#endif

/* ===== DEBUG ===== */
#if defined(LOG_LEVEL) && (LOG_LEVEL >= LOG_LEVEL_DEBUG)
  #define LOG_DEBUG(fmt, ...) Serial.printf("[DBG] " fmt "\n", ##__VA_ARGS__)
  #define LOG_DEBUG_INLINE(fmt, ...) Serial.printf("\r[DBG] " fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG(...)
  #define LOG_DEBUG_INLINE(...)
#endif