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

// IMPORTANT: These defines must come BEFORE including NTPClient.h
// In a real project, put these in platformio.ini build_flags
#define NTP_UDP_IMPLEMENTATION "EthernetUdp.h"
#define NTP_UDP_CLASS EthernetUDP

#include <ETH.h>          // ESP32 native Ethernet
#include <EthernetUdp.h>  // Your EthernetUDP implementation
#include <NTPClient.h>

// Ethernet configuration for ESP32 with LAN8720
// Adjust these for your hardware
#define ETH_PHY_ADDR        1
#define ETH_PHY_TYPE        ETH_PHY_LAN8720
#define ETH_PHY_POWER       12  // Pin# of the enable signal for the external crystal oscillator (-1 to disable)
#define ETH_PHY_MDC         23  // Pin# of the I²C-like MDC signal
#define ETH_PHY_MDIO        18  // Pin# of the I²C-like MDIO signal
#define ETH_CLK_MODE        ETH_CLOCK_GPIO17_OUT

// Use global NTP instance
extern NTPClient NTP;

// Ethernet event handler
void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_ETH_START:
            Serial.println("ETH Started");
            ETH.setHostname("esp32-ntp");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("ETH Connected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.print("ETH Got IP: ");
            Serial.println(ETH.localIP());
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("ETH Disconnected");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            Serial.println("ETH Stopped");
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== NTPClient Ethernet Example ===");
    
    // Initialize Ethernet
    WiFi.onEvent(WiFiEvent);
    ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
    
    // Wait for Ethernet to get IP
    Serial.print("Waiting for Ethernet connection");
    while (!ETH.localIP()[0]) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    // Initialize NTP client
    NTP.begin();
    
    // Set time zone (example: PST with DST)
    NTP.setTimeZone(NTPClient::getTimeZonePST());
    
    // Sync time
    Serial.println("\nSyncing time...");
    auto result = NTP.syncTime();
    
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