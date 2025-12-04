# NTPClient

Advanced NTP (Network Time Protocol) client library for ESP32 with multi-server support, automatic DST handling, RTC integration, and sophisticated retry strategies.

## Features

- **Multi-Server Support**: Manage multiple NTP servers with automatic failover
- **Server Scoring**: Intelligent server selection based on stratum, RTT, and reliability
- **Time Zone Management**: Built-in support for time zones and DST transitions
- **RTC Integration**: Seamless integration with RTC modules (like DS3231)
- **Auto-Sync**: Configurable automatic time synchronization
- **Statistics**: Track sync performance and server reliability
- **Callbacks**: Event notifications for sync events and time changes
- **Retry Strategies**: Exponential backoff and server rotation on failures
- **Temperature Compensation**: Works with temperature-compensated RTCs

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    packerlschupfer/NTPClient
    ; or for local development:
    ; NTPClient=symlink:///path/to/workspace_Class-NTPClient
```

### Arduino IDE

1. Download the library
2. Place in your Arduino libraries folder
3. No external dependencies required

## Ethernet Support

The library supports Ethernet connections without WiFi dependencies through compile-time configuration. This is useful for:
- ESP32 boards with built-in Ethernet (WT32-ETH01, ESP32-Ethernet-Kit)
- Projects requiring minimal memory footprint
- Ethernet-only implementations without WiFi stack

### Configuration

Add these build flags to your `platformio.ini`:

```ini
build_flags = 
    -DNTP_UDP_IMPLEMENTATION=\"EthernetUdp.h\"
    -DNTP_UDP_CLASS=EthernetUDP
```

Or in Arduino IDE, add before including NTPClient.h:

```cpp
#define NTP_UDP_IMPLEMENTATION "EthernetUdp.h"
#define NTP_UDP_CLASS EthernetUDP

#include <EthernetUdp.h>
#include <NTPClient.h>
```

**Note**: You need to provide an EthernetUDP implementation that matches the WiFiUDP interface. The library will use whatever UDP class you specify.

### Benefits

- No WiFi dependencies when using Ethernet
- Reduced binary size and RAM usage
- Compatible with any UDP implementation
- Maintains backward compatibility (defaults to WiFiUDP)

See `examples/ethernet/` for a complete example.

## Quick Start

```cpp
#include <WiFi.h>
#include <NTPClient.h>

// Create your own instance (no global instance)
NTPClient ntp;

void setup() {
    // Connect to WiFi first
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    // Option 1: Use default servers
    ntp.beginWithDefaults();
    
    // Option 2: Configure custom servers
    ntp.addServer("pool.ntp.org");
    ntp.addServer("time.google.com");
    ntp.begin();  // Initialize without adding defaults
    
    // Set time zone (e.g., EST with DST)
    ntp.setTimeZone(NTPClient::getTimeZoneEST());
    
    // Handle time changes gracefully
    ntp.onTimeChange([](time_t oldTime, time_t newTime) {
        int32_t diff = newTime - oldTime;
        if (oldTime < 86400) {  // Initial sync
            Serial.printf("Time synchronized: %s\n", 
                         NTPClient::epochToString(newTime).c_str());
        } else {
            Serial.printf("Time adjusted by %ld seconds\n", diff);
        }
    });
    
    // Sync time
    auto result = ntp.syncTime();
    if (result.success) {
        Serial.println("Time synced!");
        Serial.println(ntp.getFormattedDateTime());
    }
    
    // Enable auto-sync every hour
    ntp.setAutoSync(true, 3600);
}

void loop() {
    ntp.process();  // Handle auto-sync
}
```

## Server Management

### Adding Servers

```cpp
// Add custom servers (default pool is already included)
NTP.addServer("time.nist.gov");
NTP.addServer("time.google.com");
NTP.addServer("time.cloudflare.com");
NTP.addServer("ntp.ubuntu.com", 123);  // With custom port

// Remove a server
NTP.removeServer("time.nist.gov");

// Clear all servers
NTP.clearServers();
```

### Server Selection

The library automatically selects the best server based on:
- Stratum level (lower is better)
- Average round-trip time
- Failure count
- Reachability status

```cpp
// Get best server info
auto* best = NTP.getBestServer();
if (best) {
    Serial.printf("Best server: %s (stratum %d, RTT %dms)\n",
                  best->hostname.c_str(), best->stratum, best->averageRTT);
}
```

## Time Zones

### Built-in Time Zones

```cpp
// US Eastern Time
NTP.setTimeZone(NTPClient::getTimeZoneEST());

// US Pacific Time
NTP.setTimeZone(NTPClient::getTimeZonePST());

// Central European Time
NTP.setTimeZone(NTPClient::getTimeZoneCET());

// UTC (no offset)
NTP.setTimeZone(NTPClient::getTimeZoneUTC());
```

### Custom Time Zone

```cpp
NTPClient::TimeZoneConfig customTZ = {
    -300,         // UTC offset in minutes (-5 hours)
    "EST",        // Time zone name
    true,         // Use DST
    2, 3, 0, 2,   // DST start: 2nd Sunday in March at 2:00 AM
    1, 11, 0, 2,  // DST end: 1st Sunday in November at 2:00 AM
    60            // DST offset in minutes (+1 hour)
};
NTP.setTimeZone(customTZ);
```

## RTC Integration

Perfect integration with DS3231Controller or other RTC libraries:

```cpp
// Set RTC update callback
NTP.setRTCCallback([](time_t epoch) {
    // Update your RTC here
    rtc.setTime(DateTime(epoch));
    Serial.println("RTC updated!");
});

// Sync system time to RTC
NTP.syncToRTC();
```

## Automatic Synchronization

```cpp
// Enable auto-sync every 30 minutes
NTP.setAutoSync(true, 1800);

// Check auto-sync status
if (NTP.isAutoSyncEnabled()) {
    Serial.printf("Next sync in %ld seconds\n", 
                  NTP.getNextSyncTime() - time(nullptr));
}

// In loop(), call process()
void loop() {
    NTP.process();  // Handles auto-sync
}
```

## Callbacks

```cpp
// Sync event callback
NTP.onSync([](const NTPClient::SyncResult& result) {
    if (result.success) {
        Serial.printf("Synced from %s, offset %ldms\n", 
                     result.serverUsed.c_str(), result.offsetMs);
    } else {
        Serial.printf("Sync failed: %s\n", result.error.c_str());
    }
});

// Time change callback
NTP.onTimeChange([](time_t oldTime, time_t newTime) {
    int32_t diff = newTime - oldTime;
    Serial.printf("Time adjusted by %ld seconds\n", diff);
});
```

## Time Formatting

**Important**: The formatted time methods return `const char*` pointers to an internal buffer. These methods are safe and will never return NULL.

### Return Values
- **Normal operation**: Returns formatted time string
- **Time not synced**: Returns "Not Synced"
- **Invalid time**: Returns "Invalid Time"  
- **Format error**: Returns "Format Error"

```cpp
// Various formats (returns const char*, never NULL)
const char* time = NTP.getFormattedTime();           // "14:30:45" or "Not Synced"
const char* date = NTP.getFormattedDate();           // "2024-01-15" or "Not Synced"
const char* datetime = NTP.getFormattedDateTime();   // "2024-01-15 14:30:45" or "Not Synced"

// Custom format (strftime)
const char* custom = NTP.getFormattedTime("%A, %B %d, %Y %I:%M %p");
// Output: "Monday, January 15, 2024 02:30 PM" or error message

// Always safe to use in logging - never returns NULL
Serial.printf("Current time: %s\n", NTP.getFormattedDateTime());
ESP_LOGI(TAG, "Time: %s", NTP.getFormattedTime());

// Check if time is valid before using
if (strcmp(NTP.getFormattedTime(), "Not Synced") != 0) {
    // Time is valid
}

// Get raw epoch time
time_t epoch = NTP.getEpochTime();      // UTC (0 if not synced)
time_t local = NTP.getLocalTime();      // With timezone offset
```

## Statistics and Diagnostics

```cpp
// Print comprehensive diagnostics
NTP.printDiagnostics();

// Get statistics
uint32_t syncs = NTP.getSyncCount();
uint32_t failures = NTP.getSyncFailures();
float avgTime = NTP.getAverageSyncTime();
int32_t lastOffset = NTP.getLastOffset();

// Reset statistics
NTP.resetStatistics();
```

## Advanced Usage

### Watchdog-Safe Operation

The library uses non-blocking operation by default with minimal delays and yield() calls. For systems with strict watchdog requirements, you can provide a custom yield callback:

```cpp
// Define a yield callback that feeds your watchdog
void feedWatchdog() {
    esp_task_wdt_reset();  // ESP32 example
    // Do other critical tasks if needed
}

// Set the yield callback for additional control
NTP.setYieldCallback(feedWatchdog);

// All NTP operations are non-blocking by default
auto result = NTP.syncTime(10000);  // Safe for watchdog systems
```

The library automatically:
- Uses minimal 1ms delays instead of blocking 10ms delays
- Calls yield() to allow task switching
- Invokes your custom callback if provided

This ensures compatibility with:
- Embedded systems with hardware watchdogs
- FreeRTOS tasks that must feed task watchdogs
- Safety-critical applications that can't block
- Systems with real-time requirements

See `examples/watchdog_safe/` for a complete example.

### Manual Time Sync

```cpp
// Force sync (ignores recent sync check)
bool success = NTP.forceSync();

// Sync from specific server
auto result = NTP.syncTimeFromServer("time.google.com", 5000);

// Manual time adjustment
NTP.adjustTime(-3600);  // Subtract 1 hour
```

### DST Handling

```cpp
// Check if currently in DST
if (NTP.isDST()) {
    Serial.println("Daylight Saving Time is active");
}

// Check DST for specific time
time_t futureTime = NTP.makeTime(2024, 7, 1, 12, 0, 0);
if (NTP.isDST(futureTime)) {
    Serial.println("July 1st is in DST");
}
```

## Error Handling

The library provides detailed error information:

```cpp
auto result = NTP.syncTime();
if (!result.success) {
    Serial.printf("Sync failed: %s\n", result.error.c_str());
    
    // Possible errors:
    // - "NTP client not initialized"
    // - "Failed to send NTP packet"
    // - "Timeout waiting for NTP response"
    // - "Invalid NTP packet received"
    // - "Failed to sync with any server"
}
```

## Performance Considerations

- Server responses are cached with exponential moving average
- Failed servers are temporarily marked as unreachable
- Round-trip times are measured and used for server selection
- Network delays are compensated using symmetric assumption

## Debug Output

Enable debug logging:

```cpp
// In platformio.ini or build flags
build_flags = 
    -DNTP_DEBUG
    -DUSE_CUSTOM_LOGGER  ; If using custom logger
```

## Examples

See the `examples` folder for:
- `basic` - Simple time synchronization
- `with_rtc` - Integration with DS3231 RTC
- `multi_server` - Advanced multi-server setup

## API Reference

### Core Methods
- `begin(localPort)` - Initialize NTP client
- `end()` - Stop NTP client
- `syncTime(timeout)` - Sync from best available server
- `process()` - Handle auto-sync (call in loop)

### Configuration
- `setTimeZone(config)` - Set time zone configuration
- `setAutoSync(enable, interval)` - Configure automatic sync
- `addServer(hostname, port)` - Add NTP server
- `removeServer(hostname)` - Remove NTP server

### Time Methods
- `getEpochTime()` - Get UTC time
- `getLocalTime()` - Get local time with timezone
- `getFormattedTime(format)` - Get formatted time string
- `isDST()` - Check if in daylight saving time

## License

MIT License - see LICENSE file for details

## Contributing

Pull requests welcome! Please follow the existing code style and include tests.

## Acknowledgments

- Based on ESP32 time.h functionality
- Inspired by various NTP client implementations
- Designed for reliability in IoT applications