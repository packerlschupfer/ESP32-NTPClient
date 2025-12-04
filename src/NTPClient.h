#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

// No global instance - users must create their own

#include <Arduino.h>

// Allow compile-time configuration of UDP implementation
#ifndef NTP_UDP_IMPLEMENTATION
    // Default to WiFi for backward compatibility
    #include <WiFiUdp.h>
    #define NTP_UDP_CLASS WiFiUDP
#else
    // Allow users to specify their own UDP implementation
    #include NTP_UDP_IMPLEMENTATION
    #ifndef NTP_UDP_CLASS
        #error "NTP_UDP_CLASS must be defined when using custom UDP implementation"
    #endif
#endif

#include <time.h>
#include <vector>
#include <functional>
#include "NTPClientLogging.h"

class NTPClient {
public:
    // NTP packet structure
    struct NTPPacket {
        uint8_t  li_vn_mode;      // Eight bits: li(2), vn(3), mode(3)
        uint8_t  stratum;         // Stratum level of the local clock
        uint8_t  poll;            // Poll interval
        uint8_t  precision;       // Precision of the local clock
        uint32_t rootDelay;       // Total round trip delay time
        uint32_t rootDispersion;  // Max error allowed from primary clock source
        uint32_t refId;           // Reference clock identifier
        uint32_t refTm_s;         // Reference time-stamp seconds
        uint32_t refTm_f;         // Reference time-stamp fraction of a second
        uint32_t origTm_s;        // Originate time-stamp seconds
        uint32_t origTm_f;        // Originate time-stamp fraction of a second
        uint32_t rxTm_s;          // Received time-stamp seconds
        uint32_t rxTm_f;          // Received time-stamp fraction of a second
        uint32_t txTm_s;          // Transmit time-stamp seconds
        uint32_t txTm_f;          // Transmit time-stamp fraction of a second
    } __attribute__((packed));

    // Server configuration
    struct NTPServer {
        String hostname;
        uint16_t port;
        uint32_t lastSuccessTime;
        uint32_t failureCount;
        int32_t averageOffset;    // Running average offset in ms
        uint16_t averageRTT;      // Running average round-trip time in ms
        bool reachable;
        uint8_t stratum;          // Server's stratum level
    };

    // Sync result
    struct SyncResult {
        time_t syncTime;          // When sync occurred (8 bytes, aligned first)
        char serverUsed[64];      // Server hostname/IP (max 63 chars + null)
        char error[128];          // Error message if failed (max 127 chars + null)
        int32_t offsetMs;         // Time offset in milliseconds (true ms precision)
        uint32_t syncUsec;        // Microseconds component of sync time (0-999999)
        uint16_t roundTripMs;     // Round trip time in milliseconds
        uint8_t stratum;          // Server stratum
        bool success;             // Success flag

        // Constructor to initialize char arrays
        SyncResult() : syncTime(0), offsetMs(0), syncUsec(0), roundTripMs(0), stratum(0), success(false) {
            serverUsed[0] = '\0';
            error[0] = '\0';
        }
    };

    // Time zone configuration
    struct TimeZoneConfig {
        int16_t offsetMinutes;    // UTC offset in minutes
        String name;              // e.g., "EST", "PST"
        bool useDST;              // Use daylight saving time
        uint8_t dstStartWeek;     // Week of month (1-5, 5=last)
        uint8_t dstStartMonth;    // Month (1-12)
        uint8_t dstStartDayOfWeek;// Day of week (0=Sunday)
        uint8_t dstStartHour;     // Hour to start DST
        uint8_t dstEndWeek;       // Week of month (1-5, 5=last)
        uint8_t dstEndMonth;      // Month (1-12)
        uint8_t dstEndDayOfWeek;  // Day of week (0=Sunday)
        uint8_t dstEndHour;       // Hour to end DST
        int16_t dstOffsetMinutes; // Additional offset during DST
    };

    // Callbacks
    using SyncCallback = std::function<void(const SyncResult&)>;
    using TimeChangeCallback = std::function<void(time_t oldTime, time_t newTime)>;
    using YieldCallback = std::function<void()>;

    // Constructor/Destructor
    NTPClient();
    ~NTPClient() = default;

    // Configuration
    void begin(uint16_t localPort = 8888);
    void beginWithDefaults(uint16_t localPort = 8888);  // Explicitly add default servers
    void end();
    
    // Server management
    [[nodiscard]] bool addServer(const String& hostname, uint16_t port = 123);
    [[nodiscard]] bool removeServer(const String& hostname);
    void clearServers();
    [[nodiscard]] std::vector<NTPServer> getServers() const { return _servers; }
    [[nodiscard]] NTPServer* getBestServer();

    // Time synchronization
    [[nodiscard]] SyncResult syncTime(uint32_t timeoutMs = 5000);
    [[nodiscard]] SyncResult syncTimeFromServer(const String& hostname, uint32_t timeoutMs = 5000);
    [[nodiscard]] bool forceSync();

    // Automatic sync
    void setAutoSync(bool enable, uint32_t intervalSeconds = 3600);
    [[nodiscard]] bool isAutoSyncEnabled() const noexcept { return _autoSyncEnabled; }
    [[nodiscard]] uint32_t getAutoSyncInterval() const noexcept { return _autoSyncInterval; }
    [[nodiscard]] time_t getLastSyncTime() const noexcept { return _lastSyncTime; }
    [[nodiscard]] time_t getNextSyncTime() const;

    // Time zone management
    void setTimeZone(const TimeZoneConfig& config);
    [[nodiscard]] TimeZoneConfig getTimeZone() const noexcept { return _timezone; }
    [[nodiscard]] bool isDST() const;
    [[nodiscard]] bool isDST(time_t timestamp) const;
    
    // Common time zones
    static TimeZoneConfig getTimeZoneEST();  // Eastern Standard Time
    static TimeZoneConfig getTimeZonePST();  // Pacific Standard Time
    static TimeZoneConfig getTimeZoneCET();  // Central European Time
    static TimeZoneConfig getTimeZoneUTC();  // UTC (no offset)
    
    // Time getters
    [[nodiscard]] time_t getEpochTime() const;
    [[nodiscard]] time_t getLocalTime() const;
    [[nodiscard]] const char* getFormattedTime() const;
    [[nodiscard]] const char* getFormattedTime(const char* format) const;
    [[nodiscard]] const char* getFormattedDate() const;
    [[nodiscard]] const char* getFormattedDateTime() const;
    
    // Time setters (for manual adjustment)
    void setEpochTime(time_t epoch);
    void adjustTime(int32_t offsetSeconds);
    
    // RTC integration
    void setRTCCallback(std::function<void(time_t)> callback) { _rtcCallback = callback; }
    void syncToRTC();
    
    // Statistics and diagnostics
    [[nodiscard]] uint32_t getSyncCount() const noexcept { return _syncCount; }
    [[nodiscard]] uint32_t getSyncFailures() const noexcept { return _syncFailures; }
    [[nodiscard]] float getAverageSyncTime() const noexcept { return _averageSyncTime; }
    [[nodiscard]] int32_t getLastOffset() const noexcept { return _lastOffset; }
    void printDiagnostics();
    void resetStatistics();
    
    // Callbacks
    void onSync(SyncCallback callback) { _syncCallback = callback; }
    void onTimeChange(TimeChangeCallback callback) { _timeChangeCallback = callback; }
    void setYieldCallback(YieldCallback callback) { _yieldCallback = callback; }
    
    // Utility methods
    static String epochToString(time_t epoch, const char* format = "%Y-%m-%d %H:%M:%S");
    static time_t makeTime(int year, int month, int day, int hour, int minute, int second);
    static bool isLeapYear(int year);
    static uint8_t daysInMonth(int month, int year);
    
    // Process (call in loop for auto-sync)
    void process();

private:
    NTP_UDP_CLASS _udp;
    uint16_t _localPort;
    std::vector<NTPServer> _servers;
    TimeZoneConfig _timezone;
    
    // State
    bool _initialized;
    bool _autoSyncEnabled;
    uint32_t _autoSyncInterval;
    time_t _lastSyncTime;
    time_t _lastProcessTime;
    int32_t _lastOffset;
    
    // Statistics
    uint32_t _syncCount;
    uint32_t _syncFailures;
    float _averageSyncTime;
    uint32_t _totalSyncTime;
    
    // Internal buffer for formatted strings (prevents crash with c_str())
    mutable char _formattedBuffer[32];
    
    // Callbacks
    SyncCallback _syncCallback;
    TimeChangeCallback _timeChangeCallback;
    std::function<void(time_t)> _rtcCallback;
    YieldCallback _yieldCallback;
    
    // Internal methods
    bool sendNTPPacket(const String& address);
    bool receiveNTPPacket(NTPPacket& packet, uint32_t timeoutMs);
    time_t parseNTPPacket(const NTPPacket& packet, uint16_t& rtt, uint32_t& usecOut);
    void updateServerStats(NTPServer& server, bool success, int32_t offset, uint16_t rtt);
    time_t getDSTTransition(int year, uint8_t month, uint8_t week, uint8_t dayOfWeek, uint8_t hour) const;
    void applyTimeOffset(time_t newTime, uint32_t usec);
    
    // Constants
    static constexpr uint32_t NTP_TIMESTAMP_DELTA = 2208988800UL;  // 1900 to 1970
    static constexpr uint32_t MIN_SYNC_INTERVAL = 60;              // 1 minute minimum
    static constexpr uint32_t DEFAULT_NTP_PORT = 123;
    static constexpr uint8_t NTP_PACKET_SIZE = 48;
    static constexpr uint8_t MAX_SERVERS = 10;
    static constexpr uint8_t MAX_RETRY_COUNT = 3;
    static constexpr float OFFSET_FILTER_ALPHA = 0.1f;  // Exponential moving average filter
    
    // Default NTP servers
    static const char* DEFAULT_NTP_SERVERS[];
    static const uint8_t DEFAULT_SERVER_COUNT;
};

// No global instance - create your own:
// NTPClient ntp;

#endif // NTP_CLIENT_H