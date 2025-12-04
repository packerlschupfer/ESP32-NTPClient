#ifndef NTPCLIENT_LOGGING_H
#define NTPCLIENT_LOGGING_H

#define NTP_LOG_TAG "NTPClient"

#include <esp_log.h>  // Required for ESP_LOG_* constants

// Define log levels based on debug flag
#ifdef NTP_DEBUG
    // Debug mode: Show all levels
    #define NTP_LOG_LEVEL_E ESP_LOG_ERROR
    #define NTP_LOG_LEVEL_W ESP_LOG_WARN
    #define NTP_LOG_LEVEL_I ESP_LOG_INFO
    #define NTP_LOG_LEVEL_D ESP_LOG_DEBUG
    #define NTP_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    // Release mode: Only Error, Warn, Info
    #define NTP_LOG_LEVEL_E ESP_LOG_ERROR
    #define NTP_LOG_LEVEL_W ESP_LOG_WARN
    #define NTP_LOG_LEVEL_I ESP_LOG_INFO
    #define NTP_LOG_LEVEL_D ESP_LOG_NONE  // Suppress
    #define NTP_LOG_LEVEL_V ESP_LOG_NONE  // Suppress
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include <Logger.h>
    #ifndef NTP_LOG_E
        #define NTP_LOG_E(...) Logger::getInstance().log(NTP_LOG_LEVEL_E, NTP_LOG_TAG, __VA_ARGS__)
    #endif
    #ifndef NTP_LOG_W
        #define NTP_LOG_W(...) Logger::getInstance().log(NTP_LOG_LEVEL_W, NTP_LOG_TAG, __VA_ARGS__)
    #endif
    #ifndef NTP_LOG_I
        #define NTP_LOG_I(...) Logger::getInstance().log(NTP_LOG_LEVEL_I, NTP_LOG_TAG, __VA_ARGS__)
    #endif
    #ifndef NTP_LOG_D
        #ifdef NTP_DEBUG
            #define NTP_LOG_D(...) Logger::getInstance().log(NTP_LOG_LEVEL_D, NTP_LOG_TAG, __VA_ARGS__)
        #else
            #define NTP_LOG_D(...) ((void)0)
        #endif
    #endif
    #ifndef NTP_LOG_V
        #ifdef NTP_DEBUG
            #define NTP_LOG_V(...) Logger::getInstance().log(NTP_LOG_LEVEL_V, NTP_LOG_TAG, __VA_ARGS__)
        #else
            #define NTP_LOG_V(...) ((void)0)
        #endif
    #endif
#else
    // Use ESP-IDF logging with compile-time suppression
    #ifndef NTP_LOG_E
        #define NTP_LOG_E(...) ESP_LOGE(NTP_LOG_TAG, __VA_ARGS__)
    #endif
    #ifndef NTP_LOG_W
        #define NTP_LOG_W(...) ESP_LOGW(NTP_LOG_TAG, __VA_ARGS__)
    #endif
    #ifndef NTP_LOG_I
        #define NTP_LOG_I(...) ESP_LOGI(NTP_LOG_TAG, __VA_ARGS__)
    #endif
    #ifndef NTP_LOG_D
        #ifdef NTP_DEBUG
            #define NTP_LOG_D(...) ESP_LOGD(NTP_LOG_TAG, __VA_ARGS__)
        #else
            #define NTP_LOG_D(...) ((void)0)
        #endif
    #endif
    #ifndef NTP_LOG_V
        #ifdef NTP_DEBUG
            #define NTP_LOG_V(...) ESP_LOGV(NTP_LOG_TAG, __VA_ARGS__)
        #else
            #define NTP_LOG_V(...) ((void)0)
        #endif
    #endif
#endif

// Specific log helpers for common NTP operations
#define NTP_LOG_SYNC_SUCCESS(server, offset) \
    NTP_LOG_I("Time synchronized from %s, offset: %ldms", server, offset)

#define NTP_LOG_SYNC_FAILED(server, reason) \
    NTP_LOG_W("Failed to sync with %s: %s", server, reason)

#define NTP_LOG_SERVER_STATS(server, rtt, offset) \
    NTP_LOG_D("Server %s - RTT: %dms, Offset: %ldms", server, rtt, offset)

#endif // NTPCLIENT_LOGGING_H