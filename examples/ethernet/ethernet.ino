/**
 * NTPClient Ethernet Example
 * 
 * This example demonstrates using NTPClient with Ethernet instead of WiFi.
 * It requires defining compile-time flags to use EthernetUDP.
 * 
 * Compile flags needed (in platformio.ini or build flags):
 * -DNTP_UDP_IMPLEMENTATION=\"EthernetUdp.h\"
 * -DNTP_UDP_CLASS=EthernetUDP
 * 
 * Hardware:
 * - ESP32 with Ethernet (e.g., WT32-ETH01, ESP32-Ethernet-Kit)
 * - Or ESP32 with external Ethernet module (W5500, ENC28J60, etc.)
 */

// IMPORTANT: These defines must come BEFORE including NTPClient.h.
// In a real project, put these in platformio.ini build_flags (this example
// already does so) so that the library is compiled against EthernetUDP too.
#ifndef NTP_UDP_IMPLEMENTATION
#define NTP_UDP_IMPLEMENTATION "EthernetUdp.h"
#define NTP_UDP_CLASS EthernetUDP
#endif

#include <Ethernet.h>     // Arduino Ethernet library (W5100/W5500 SPI module)
#include <EthernetUdp.h>  // EthernetUDP implementation used by NTPClient
#include <NTPClient.h>

// Ethernet configuration (W5100/W5500 SPI module)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);  // Optional static IP fallback

// Create NTP client instance
NTPClient NTP;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== NTPClient Ethernet Example ===");

    // Initialize Ethernet (DHCP first, static IP fallback)
    Serial.print("Initializing Ethernet");
    if (Ethernet.begin(mac) == 0) {
        Serial.println("\nDHCP failed, using static IP");
        Ethernet.begin(mac, ip);
    }
    Serial.print("IP address: ");
    Serial.println(Ethernet.localIP());

    // Initialize NTP client
    NTP.begin();
    
    // Set time zone (example: PST with DST)
    NTP.setTimeZone(NTPClient::getTimeZonePST());
    
    // Sync time
    Serial.println("\nSyncing time...");
    auto result = NTP.syncTime();
    
    if (result.success) {
        Serial.println("Time synchronized successfully!");
        Serial.printf("Server: %s\n", result.serverUsed);
        Serial.printf("Offset: %ldms\n", result.offsetMs);
        Serial.printf("Round trip: %dms\n", result.roundTripMs);
        Serial.printf("Server stratum: %d\n", result.stratum);
    } else {
        Serial.printf("Failed to sync time: %s\n", result.error);
    }
    
    // Enable auto-sync every 30 minutes
    NTP.setAutoSync(true, 1800);
    
    // Display current time
    Serial.println("\nCurrent time:");
    Serial.printf("UTC: %s\n", NTPClient::epochToString(NTP.getEpochTime()).c_str());
    Serial.printf("Local: %s %s\n", NTP.getFormattedDateTime(), 
                  NTP.getTimeZone().name.c_str());
    Serial.printf("DST active: %s\n", NTP.isDST() ? "Yes" : "No");
    
    Serial.println("\nSetup complete!");
}

void loop() {
    // Let NTP client handle auto-sync
    NTP.process();
    
    // Print time every 10 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 10000) {
        lastPrint = millis();
        
        Serial.printf("\nTime: %s", NTP.getFormattedTime());
        
        // Show if we're in DST
        if (NTP.getTimeZone().useDST) {
            Serial.printf(" (%s)", NTP.isDST() ? "DST" : "Standard");
        }
        
        Serial.println();
    }
    
    delay(100);
}