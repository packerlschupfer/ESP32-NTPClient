/*
 * NTPClientLogging.h - part of the ESP32-NTPClient library
 *
 * Copyright (C) 2025-2026 packerlschupfer
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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