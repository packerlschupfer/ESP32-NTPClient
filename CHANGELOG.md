# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-12-04

### Added
- Initial public release
- Multi-server NTP client with automatic failover
- Support for 4+ NTP servers with health-based selection
- Automatic DST (Daylight Saving Time) handling
- Built-in timezone configurations (UTC, EST, PST, CET, etc.)
- RTC (Real-Time Clock) integration for backup time source
- DS3231 temperature-compensated RTC support
- Exponential moving average for time offset filtering
- Server health metrics (RTT, offset, stratum, success rate)
- Callback system for sync events and time changes
- Automatic background synchronization with configurable intervals
- TZ environment variable support (POSIX timezone strings)
- NTPv4 protocol implementation
- Non-blocking UDP communication
- Drift tracking and compensation

Platform: ESP32 (Arduino/ESP-IDF)
License: MIT
Dependencies: None (uses ESP32 WiFi/Ethernet)

### Notes
- Production-tested with 4-server configuration (local gateway + pool.ntp.org + time.google.com + time.cloudflare.com)
- Stable time synchronization with RTC fallback
- Previous internal versions (v1.x) not publicly released
- Reset to v0.1.0 for clean public release start
