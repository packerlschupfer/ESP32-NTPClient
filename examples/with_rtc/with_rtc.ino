/**
 * NTPClient with RTC Integration Example
 *
 * This example demonstrates using NTPClient with DS3231Controller
 * to maintain accurate time with automatic RTC synchronization.
 *
 * Features:
 * - Sync time from NTP servers
 * - Automatically update RTC when time is synced
 * - Use RTC as backup when network is unavailable
 * - Handle time zone and DST with proper UTC conversion
 * - Thread-safe RTC operations (safe for multi-task usage)
 *
 * Hardware:
 * - ESP32 with Ethernet
 * - DS3231 RTC module
 *
 * Connections:
 * - DS3231 SDA -> ESP32 GPIO 21
 * - DS3231 SCL -> ESP32 GPIO 22
 *
 * Note: DS3231 has 1-second resolution, while NTP provides millisecond
 * precision. The RTC is used as a backup time source, not for precise timing.
 */

#include <Ethernet.h>
#include <NTPClient.h>
#include <DS3231Controller.h>

// Ethernet configuration
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);  // Optional: Set static IP, or use DHCP

// Create instances
NTPClient ntp;
DS3231Controller rtc;

// State tracking
bool ethernetConnected = false;
unsigned long lastSyncAttempt = 0;
const unsigned long SYNC_RETRY_INTERVAL = 300000;  // 5 minutes

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== NTP + RTC Integration Example ===");
    
    // Initialize I2C for RTC
    Wire.begin();
    
    // Initialize RTC
    if (!rtc.begin()) {
        Serial.println("ERROR: Failed to initialize DS3231!");
        while (1) delay(1000);
    }
    
    Serial.println("RTC initialized");
    
    // Get current RTC time and display it
    DateTime rtcTime = rtc.now();
    Serial.print("RTC Time: ");
    Serial.println(rtcTime.timestamp(DateTime::TIMESTAMP_FULL));

    // Set system time from RTC initially using the new helper
    // This properly sets both seconds and clears microseconds
    // Note: RTC only has 1-second resolution; microseconds will be set to 0
    if (rtc.syncSystemTime()) {
        Serial.println("System time initialized from RTC");
    } else {
        Serial.println("WARNING: Failed to sync system time from RTC");
    }
    
    // Connect to Ethernet
    connectEthernet();
    
    // Initialize NTP client
    ntp.begin();
    
    // Configure time zone (example: EST with DST)
    ntp.setTimeZone(NTPClient::getTimeZoneEST());
    
    // Set up RTC sync callback with proper timezone handling
    // NTP provides UTC epoch; RTC should store UTC for consistency
    // The DS3231Controller is thread-safe, so this callback can be called
    // from any task (e.g., if NTP sync runs on a background task)
    ntp.setRTCCallback([](time_t epoch) {
        Serial.println("Syncing time to RTC...");

        // Store UTC time in RTC (recommended for consistency)
        // Use setTimeFromUTC with offset=0 to store as UTC
        if (rtc.setTimeFromUTC(epoch, 0)) {
            DateTime dt(epoch);
            Serial.printf("RTC updated to UTC: %s\n",
                         dt.timestamp(DateTime::TIMESTAMP_FULL).c_str());

            // Show RTC temperature (useful for drift compensation analysis)
            auto temp = rtc.getTemperature();
            Serial.printf("RTC Temperature: %.1f°C\n", temp.celsius);
        } else {
            Serial.println("ERROR: Failed to update RTC!");
        }
    });
    
    // Set up sync event callback
    // Note: serverUsed and error are char arrays, not String objects
    ntp.onSync([](const NTPClient::SyncResult& result) {
        if (result.success) {
            Serial.printf("Time synced from %s\n", result.serverUsed);
            Serial.printf("  Offset: %ldms (usec: %lu), RTT: %dms, Stratum: %d\n",
                         result.offsetMs, result.syncUsec, result.roundTripMs, result.stratum);
        } else {
            Serial.printf("Sync failed: %s\n", result.error);
        }
    });
    
    // Set up time change callback
    ntp.onTimeChange([](time_t oldTime, time_t newTime) {
        int32_t diff = newTime - oldTime;
        Serial.printf("Time adjusted by %ld seconds\n", diff);
    });
    
    // Enable auto-sync every hour
    ntp.setAutoSync(true, 3600);
    
    // Perform initial sync if Ethernet is connected
    if (ethernetConnected) {
        Serial.println("\nPerforming initial time sync...");
        auto result = ntp.syncTime();
        
        if (!result.success) {
            Serial.println("Initial sync failed, will retry later");
        }
    }
    
    // Print diagnostics
    ntp.printDiagnostics();
    rtc.printDiagnostics();
    
    Serial.println("\nSetup complete!");
    Serial.println("Commands:");
    Serial.println("  s - Sync time now");
    Serial.println("  t - Show current time");
    Serial.println("  d - Show diagnostics");
    Serial.println("  r - Show RTC time");
    Serial.println("  e - Reconnect Ethernet");
}

void loop() {
    // Process auto-sync
    ntp.process();
    
    // Check Ethernet and attempt reconnect if needed
    if (Ethernet.linkStatus() == LinkOFF && ethernetConnected) {
        ethernetConnected = false;
        Serial.println("Ethernet connection lost!");
    }
    
    // Retry sync periodically if it failed
    if (!ethernetConnected || ntp.getLastSyncTime() == 0) {
        if (millis() - lastSyncAttempt > SYNC_RETRY_INTERVAL) {
            lastSyncAttempt = millis();
            
            if (!ethernetConnected) {
                connectEthernet();
            }
            
            if (ethernetConnected) {
                Serial.println("Retrying time sync...");
                ntp.syncTime();
            }
        }
    }
    
    // Handle serial commands
    handleSerialCommands();
    
    // Small delay
    delay(100);
}

void connectEthernet() {
    Serial.print("Connecting to Ethernet");
    
    // Try DHCP first
    if (Ethernet.begin(mac) == 0) {
        Serial.println("\nFailed to configure Ethernet using DHCP");
        // Try with static IP
        Ethernet.begin(mac, ip);
    }
    
    // Check link status
    if (Ethernet.linkStatus() == LinkON) {
        ethernetConnected = true;
        Serial.println("\nEthernet connected!");
        Serial.print("IP address: ");
        Serial.println(Ethernet.localIP());
    } else {
        Serial.println("\nEthernet connection failed!");
        Serial.println("Will use RTC time only");
    }
}

void handleSerialCommands() {
    if (!Serial.available()) return;
    
    char cmd = Serial.read();
    while (Serial.available()) Serial.read();  // Clear buffer
    
    switch (cmd) {
        case 's': {
            // Force sync
            Serial.println("\nForcing time sync...");
            if (ethernetConnected) {
                auto result = ntp.syncTime();
                if (!result.success) {
                    Serial.printf("Sync failed: %s\n", result.error);
                }
            } else {
                Serial.println("No Ethernet connection!");
            }
            break;
        }
        
        case 't': {
            // Show current time
            Serial.println("\n--- Current Time ---");
            Serial.printf("System Time (UTC): %s\n", 
                         NTPClient::epochToString(ntp.getEpochTime()).c_str());
            Serial.printf("Local Time: %s %s\n", 
                         ntp.getFormattedDateTime(),
                         ntp.getTimeZone().name.c_str());
            Serial.printf("DST Active: %s\n", ntp.isDST() ? "Yes" : "No");
            
            if (ntp.getLastSyncTime() > 0) {
                time_t age = time(nullptr) - ntp.getLastSyncTime();
                Serial.printf("Last sync: %ld seconds ago\n", age);
                Serial.printf("Next sync in: %ld seconds\n", 
                             ntp.getNextSyncTime() - time(nullptr));
            } else {
                Serial.println("Never synced");
            }
            break;
        }
        
        case 'd': {
            // Show diagnostics
            Serial.println();
            ntp.printDiagnostics();
            break;
        }
        
        case 'r': {
            // Show RTC time
            DateTime rtcTime = rtc.now();
            Serial.println("\n--- RTC Status ---");
            Serial.printf("RTC Time: %s\n", 
                         rtcTime.timestamp(DateTime::TIMESTAMP_FULL).c_str());
            
            // Compare with system time
            time_t sysTime = time(nullptr);
            int32_t diff = rtcTime.unixtime() - sysTime;
            Serial.printf("RTC vs System: %+ld seconds\n", diff);
            
            // Show temperature
            auto temp = rtc.getTemperature();
            Serial.printf("Temperature: %.1f°C / %.1f°F\n", 
                         temp.celsius, temp.fahrenheit);
            break;
        }
        
        case 'e': {
            // Reconnect Ethernet
            Serial.println("\nReconnecting Ethernet...");
            Ethernet.end();
            delay(1000);
            connectEthernet();
            break;
        }
        
        default:
            Serial.println("\nCommands:");
            Serial.println("  s - Sync time now");
            Serial.println("  t - Show current time");
            Serial.println("  d - Show diagnostics");
            Serial.println("  r - Show RTC time");
            Serial.println("  e - Reconnect Ethernet");
            break;
    }
}