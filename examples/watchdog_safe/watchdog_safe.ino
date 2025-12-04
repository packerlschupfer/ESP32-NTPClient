/**
 * NTPClient Watchdog-Safe Example
 * 
 * This example demonstrates how to use NTPClient with watchdog timers.
 * The library is non-blocking by default, but you can provide a custom
 * yield callback for additional watchdog feeding and critical tasks.
 * 
 * The library automatically uses minimal delays and yield() calls,
 * making it safe for watchdog-enabled systems out of the box.
 * 
 * Hardware: ESP32 with WiFi
 */

#include <WiFi.h>
#include <NTPClient.h>
#include <esp_task_wdt.h>

// WiFi credentials
const char* ssid = "your-ssid";
const char* password = "your-password";

// Watchdog timeout (seconds)
#define WDT_TIMEOUT 10

// Use global NTP instance
extern NTPClient NTP;

// Watchdog-safe yield function
void feedWatchdog() {
    // Feed the watchdog
    esp_task_wdt_reset();
    
    // You can also do other critical tasks here
    // For example: check critical sensors, handle safety systems, etc.
    
    // Note: The library already does delay(1) and yield() internally,
    // so this callback is purely for additional watchdog feeding
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== NTPClient Watchdog-Safe Example ===");
    
    // Initialize watchdog
    esp_task_wdt_init(WDT_TIMEOUT, true);  // Enable panic so ESP32 restarts
    esp_task_wdt_add(NULL);  // Add current thread to WDT watch
    
    // Connect to WiFi
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
        esp_task_wdt_reset();  // Feed watchdog during WiFi connection
    }
    
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Initialize NTP client
    NTP.begin();
    
    // Optional: Set yield callback for additional watchdog feeding
    // The library is already non-blocking, but this gives extra control
    NTP.setYieldCallback(feedWatchdog);
    
    // Set time zone
    NTP.setTimeZone(NTPClient::getTimeZonePST());
    
    // Sync time with watchdog safety
    Serial.println("\nSyncing time (watchdog-safe)...");
    auto result = NTP.syncTime();
    
    if (result.success) {
        Serial.println("Time synchronized successfully!");
        Serial.printf("Server: %s\n", result.serverUsed.c_str());
        Serial.printf("Offset: %ldms\n", result.offsetMs);
        Serial.printf("Round trip: %dms\n", result.roundTripMs);
    } else {
        Serial.printf("Failed to sync time: %s\n", result.error.c_str());
    }
    
    // Enable auto-sync
    NTP.setAutoSync(true, 3600);
    
    Serial.println("\nSetup complete!");
    Serial.println("Watchdog is active - the device will reset if blocked for too long");
}

void loop() {
    // Feed watchdog regularly
    esp_task_wdt_reset();
    
    // Let NTP client handle auto-sync (also watchdog-safe)
    NTP.process();
    
    // Simulate some work
    static unsigned long lastWork = 0;
    if (millis() - lastWork > 5000) {
        lastWork = millis();
        
        Serial.printf("\nTime: %s", NTP.getFormattedDateTime());
        Serial.println(" - Watchdog fed, system healthy");
        
        // You can safely call syncTime() anytime without worrying about watchdog
        // because the yield callback will keep it fed
    }
    
    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        while (Serial.available()) Serial.read();  // Clear buffer
        
        switch (cmd) {
            case 's':
                Serial.println("\nForcing sync (watchdog-safe)...");
                esp_task_wdt_reset();
                
                // This won't trigger watchdog even with long timeouts
                // because our yield callback feeds it
                auto result = NTP.syncTime(10000);  // 10 second timeout
                
                if (result.success) {
                    Serial.println("Sync successful!");
                } else {
                    Serial.println("Sync failed!");
                }
                break;
        }
    }
    
    delay(100);
}

/**
 * Advanced Usage: Custom yield callback with multiple responsibilities
 * 
 * void customYield() {
 *     // 1. Feed watchdog
 *     esp_task_wdt_reset();
 *     
 *     // 2. Check critical sensors
 *     if (emergencySensor.isTriggered()) {
 *         handleEmergency();
 *     }
 *     
 *     // 3. Update critical outputs
 *     updateSafetyRelays();
 *     
 *     // 4. Allow other tasks to run
 *     vTaskDelay(1);  // FreeRTOS delay
 * }
 * 
 * // Then use it:
 * NTP.setYieldCallback(customYield);
 */