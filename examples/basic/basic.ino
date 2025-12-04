/**
 * NTPClient Basic Example
 * 
 * This example demonstrates basic usage of the NTPClient library:
 * - Connect to Ethernet
 * - Sync time from NTP servers
 * - Display formatted time
 * - Handle time zones
 * 
 * Hardware: ESP32 with Ethernet
 */

#include <Ethernet.h>
#include <NTPClient.h>

// Ethernet configuration
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);  // Optional: Set static IP, or use DHCP

// Create NTP client instance
NTPClient ntp;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== NTPClient Basic Example ===");
    
    // Initialize Ethernet
    Serial.print("Initializing Ethernet");
    
    // Try DHCP first
    if (Ethernet.begin(mac) == 0) {
        Serial.println("\nFailed to configure Ethernet using DHCP");
        // Try with static IP
        Ethernet.begin(mac, ip);
    }
    
    Serial.println("\nEthernet connected!");
    Serial.print("IP address: ");
    Serial.println(Ethernet.localIP());
    
    // Initialize NTP client with default servers
    ntp.beginWithDefaults();
    
    // Optional: Add custom NTP servers (default servers are already included)
    // NTP.addServer("time.windows.com");
    // NTP.addServer("time.apple.com");
    
    // Set time zone (example: PST with DST)
    ntp.setTimeZone(NTPClient::getTimeZonePST());
    
    // Or create custom time zone
    // NTPClient::TimeZoneConfig myTZ = {
    //     -300,      // UTC-5 hours (EST)
    //     "EST",     // Name
    //     true,      // Use DST
    //     2, 3, 0, 2,   // DST starts: 2nd Sunday in March at 2:00 AM
    //     1, 11, 0, 2,  // DST ends: 1st Sunday in November at 2:00 AM
    //     60         // DST offset: +1 hour
    // };
    // NTP.setTimeZone(myTZ);
    
    // Set up time change callback to handle initial sync gracefully
    ntp.onTimeChange([](time_t oldTime, time_t newTime) {
        int32_t diff = newTime - oldTime;
        
        if (oldTime < 86400) {  // Less than 1 day since epoch (1970)
            Serial.printf("Initial time sync completed - set to %s\n", 
                         NTPClient::epochToString(newTime).c_str());
        } else if (abs(diff) > 3600) {  // More than 1 hour
            Serial.printf("Large time adjustment: %ld seconds (%.1f hours)\n", 
                         diff, diff / 3600.0);
        } else {
            Serial.printf("Time adjusted by %ld seconds\n", diff);
        }
    });
    
    // Sync time
    Serial.println("\nSyncing time...");
    auto result = ntp.syncTime();
    
    if (result.success) {
        Serial.println("Time synchronized successfully!");
        Serial.printf("Server: %s\n", result.serverUsed.c_str());
        Serial.printf("Offset: %ldms\n", result.offsetMs);
        Serial.printf("Round trip: %dms\n", result.roundTripMs);
        Serial.printf("Server stratum: %d\n", result.stratum);
    } else {
        Serial.printf("Failed to sync time: %s\n", result.error.c_str());
    }
    
    // Enable auto-sync every 30 minutes
    ntp.setAutoSync(true, 1800);
    
    // Display current time
    Serial.println("\nCurrent time:");
    Serial.printf("UTC: %s\n", NTPClient::epochToString(ntp.getEpochTime()).c_str());
    Serial.printf("Local: %s %s\n", ntp.getFormattedDateTime(), 
                  ntp.getTimeZone().name.c_str());
    Serial.printf("DST active: %s\n", ntp.isDST() ? "Yes" : "No");
    
    Serial.println("\nSetup complete!");
    Serial.println("Enter 't' to show time, 's' to sync");
}

void loop() {
    // Let NTP client handle auto-sync
    ntp.process();
    
    // Print time every 10 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 10000) {
        lastPrint = millis();
        
        Serial.printf("\nTime: %s", ntp.getFormattedTime());
        
        // Show if we're in DST
        if (ntp.getTimeZone().useDST) {
            Serial.printf(" (%s)", ntp.isDST() ? "DST" : "Standard");
        }
        
        Serial.println();
    }
    
    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        while (Serial.available()) Serial.read();  // Clear buffer
        
        switch (cmd) {
            case 't':
                Serial.println("\n--- Current Time ---");
                Serial.printf("Date: %s\n", ntp.getFormattedDate());
                Serial.printf("Time: %s\n", ntp.getFormattedTime());
                Serial.printf("Full: %s\n", ntp.getFormattedDateTime());
                Serial.printf("Custom: %s\n", 
                             ntp.getFormattedTime("%A, %B %d, %Y %I:%M %p"));
                Serial.printf("Unix timestamp: %ld\n", ntp.getEpochTime());
                break;
                
            case 's':
                Serial.println("\nSyncing time...");
                if (ntp.forceSync()) {
                    Serial.println("Sync successful!");
                } else {
                    Serial.println("Sync failed!");
                }
                break;
                
            default:
                Serial.println("Commands: t=show time, s=sync");
        }
    }
    
    delay(100);
}