# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an advanced NTP (Network Time Protocol) client library for ESP32 microcontrollers using the Arduino framework. It provides sophisticated time synchronization with features like multi-server support, automatic DST handling, RTC integration, and smart retry strategies.

## Development Environment

- **Platform**: ESP32 (espressif32)
- **Framework**: Arduino
- **Build System**: PlatformIO
- **Language**: C++

## Common Development Commands

### Building Examples
Since this is a library without a root platformio.ini, development typically involves creating a test project:

```bash
# Create a new PlatformIO project for testing
pio init -b esp32dev

# Install this library locally for development
pio lib install file://path/to/workspace_Class-NTPClient

# Build an example
cd examples/basic
pio ci basic.ino --lib="../.." -b esp32dev
```

### Debugging
Enable debug output by adding build flags:
```bash
# In platformio.ini or command line
-DNTP_DEBUG
-DUSE_CUSTOM_LOGGER  # Optional: for custom logging
```

## Architecture Overview

### Core Components

1. **NTPClient Class** (`src/NTPClient.h`, `src/NTPClient.cpp`)
   - Main API class that handles all NTP operations
   - Manages multiple NTP servers with failover
   - Implements NTP protocol (UDP port 123)
   - Tracks server health metrics (RTT, offset, stratum)
   - Provides both synchronous and callback-based interfaces

2. **Time Zone Management**
   - Built-in timezone configs (UTC, EST, PST, CET, etc.)
   - DST (Daylight Saving Time) rules with automatic transitions
   - Configurable via `TimeZoneConfig` structure

3. **RTC Integration**
   - Optional integration with DS3231 and other I2C RTC modules
   - Automatic RTC sync after successful NTP sync
   - Temperature compensation support for DS3231

4. **Logging System** (`src/NTPClientLogging.h`)
   - Configurable log levels (ERROR, WARN, INFO, DEBUG, VERBOSE)
   - Can use ESP-IDF logging or custom logger
   - Compile-time enable/disable via `NTP_DEBUG` flag

### Key Design Patterns

1. **Global Instance Pattern**: The library provides a global `NTP` instance for convenience, but also supports creating custom instances.

2. **Server Pool Management**: Multiple NTP servers can be configured with automatic failover based on server health metrics.

3. **Exponential Moving Average**: Used for filtering time offset measurements to reduce jitter.

4. **Callback System**: Supports callbacks for sync events and time changes, enabling reactive programming patterns.

## Important Implementation Details

- The library uses non-blocking UDP communication
- NTP packets follow the standard NTPv4 protocol structure
- Time calculations handle 32-bit overflow for NTP timestamps
- Server selection uses weighted scoring based on stratum, RTT, and success rate
- Auto-sync runs in the background using ESP32 timers

## Testing Approach

The library uses example sketches as integration tests:
- `examples/basic/`: Basic time synchronization
- `examples/with_rtc/`: RTC integration example
- `examples/multi_server/`: Multiple server configuration (empty, needs implementation)

No unit test framework is currently implemented. Testing is done manually by uploading examples to ESP32 hardware.