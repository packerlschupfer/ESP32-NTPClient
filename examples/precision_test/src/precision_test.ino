/**
 * @file precision_test.ino
 * @brief Integration test for NTP millisecond precision
 *
 * This example verifies that the NTP client correctly:
 * 1. Extracts fractional seconds from NTP packets
 * 2. Sets system time with microsecond precision
 * 3. Reports accurate millisecond offsets
 *
 * Expected Results (after fix):
 * - syncUsec should be non-zero (varies with each sync)
 * - System time tv_usec should be non-zero after sync
 * - Offsets should vary smoothly (e.g., 234ms, 567ms) not quantized (0ms, 1000ms)
 */

#include <NTPClient.h>
#include <WiFi.h>
#include <sys/time.h>

// WiFi credentials - update these for your network
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

NTPClient ntp;

void setup() {
    Serial.begin(921600);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("NTP Precision Test");
    Serial.println("Testing millisecond precision fix");
    Serial.println("========================================\n");

    // Connect to WiFi
    Serial.printf("Connecting to WiFi '%s'...", ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println(" Connected!");
    Serial.printf("IP Address: %s\n\n", WiFi.localIP().toString().c_str());

    // Initialize NTP client
    ntp.begin();

    // Add NTP servers - use local server first if available
    // ntp.addServer("192.168.20.1", 123);  // Local NTP server
    ntp.addServer("pool.ntp.org", 123);
    ntp.addServer("time.google.com", 123);

    Serial.println("NTP client initialized. Starting precision tests...\n");
}

void loop() {
    static uint32_t syncCount = 0;
    static int32_t lastOffset = 0;

    Serial.printf("\n--- Sync #%lu ---\n", ++syncCount);

    // Perform NTP sync
    auto result = ntp.syncTime(5000);

    if (result.success) {
        // Check syncUsec - should be non-zero for proper precision
        bool usecOk = (result.syncUsec > 0 && result.syncUsec < 1000000);

        Serial.printf("Sync OK: %ld.%06lu\n", result.syncTime, result.syncUsec);
        Serial.printf("  Server: %s (stratum %d)\n", result.serverUsed, result.stratum);
        Serial.printf("  Offset: %ldms, RTT: %dms\n", result.offsetMs, result.roundTripMs);
        Serial.printf("  syncUsec: %lu [%s]\n", result.syncUsec,
                     usecOk ? "OK - non-zero" : "WARNING - may be zero");

        // Verify system time has microseconds
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        bool sysUsecOk = (tv.tv_usec != 0);

        Serial.printf("  System time: %ld.%06ld [%s]\n", tv.tv_sec, tv.tv_usec,
                     sysUsecOk ? "OK" : "FAIL - usec is zero!");

        // Check for quantization (old bug symptom)
        int32_t offsetDiff = abs(result.offsetMs - lastOffset);
        bool notQuantized = (result.offsetMs != 0 && result.offsetMs != 1000 &&
                            result.offsetMs != -1000);

        if (syncCount > 1) {
            Serial.printf("  Offset change: %ldms", offsetDiff);
            if (offsetDiff == 1000 || offsetDiff == 2000) {
                Serial.println(" [WARNING - may be quantized]");
            } else {
                Serial.println(" [OK - smooth variation]");
            }
        }

        lastOffset = result.offsetMs;

        // Summary for this sync
        Serial.println("\n  Test Results:");
        Serial.printf("    [%s] syncUsec populated\n", usecOk ? "PASS" : "FAIL");
        Serial.printf("    [%s] System usec set\n", sysUsecOk ? "PASS" : "FAIL");
        Serial.printf("    [%s] Offset not quantized\n", notQuantized ? "PASS" : "WARN");

    } else {
        Serial.printf("Sync FAILED: %s\n", result.error);
    }

    // Wait before next sync
    Serial.println("\nWaiting 10 seconds before next sync...");
    delay(10000);
}
