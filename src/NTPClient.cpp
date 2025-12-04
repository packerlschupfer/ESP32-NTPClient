#include "NTPClient.h"
#include <sys/time.h>

// Default NTP servers
const char* NTPClient::DEFAULT_NTP_SERVERS[] = {
    "pool.ntp.org",
    "time.nist.gov",
    "time.google.com",
    "time.cloudflare.com"
};
const uint8_t NTPClient::DEFAULT_SERVER_COUNT = 4;

// No global instance - users must create their own

NTPClient::NTPClient() 
    : _localPort(8888),
      _initialized(false),
      _autoSyncEnabled(false),
      _autoSyncInterval(3600),
      _lastSyncTime(0),
      _lastProcessTime(0),
      _lastOffset(0),
      _syncCount(0),
      _syncFailures(0),
      _averageSyncTime(0),
      _totalSyncTime(0) {
    
    // Initialize with UTC
    _timezone = getTimeZoneUTC();
}

void NTPClient::begin(uint16_t localPort) {
    _localPort = localPort;
    _udp.begin(_localPort);
    _initialized = true;
    
    NTP_LOG_I("NTP Client initialized on port %d", _localPort);
    
    if (_servers.empty()) {
        NTP_LOG_W("No NTP servers configured. Add servers or use beginWithDefaults()");
    }
}

void NTPClient::beginWithDefaults(uint16_t localPort) {
    // Add default servers before initialization
    if (_servers.empty()) {
        NTP_LOG_I("Adding default NTP servers");
        for (uint8_t i = 0; i < DEFAULT_SERVER_COUNT; i++) {
            (void)addServer(DEFAULT_NTP_SERVERS[i]);
        }
    }
    
    // Initialize normally
    begin(localPort);
}

void NTPClient::end() {
    _udp.stop();
    _initialized = false;
    NTP_LOG_I("NTP Client stopped");
}

bool NTPClient::addServer(const String& hostname, uint16_t port) {
    // Allow adding servers before begin() for pre-configuration
    if (_servers.size() >= MAX_SERVERS) {
        NTP_LOG_E("Maximum number of servers (%d) reached", MAX_SERVERS);
        return false;
    }
    
    // Check if already exists
    for (const auto& server : _servers) {
        if (server.hostname == hostname && server.port == port) {
            NTP_LOG_D("Server %s:%d already exists, skipping", hostname.c_str(), port);
            return true;  // Not an error, server is available
        }
    }
    
    NTPServer server;
    server.hostname = hostname;
    server.port = port;
    server.lastSuccessTime = 0;
    server.failureCount = 0;
    server.averageOffset = 0;
    server.averageRTT = 0;
    server.reachable = true;
    server.stratum = 255;
    
    _servers.push_back(server);
    
    NTP_LOG_I("Added NTP server %s:%d", hostname.c_str(), port);
    return true;
}

bool NTPClient::removeServer(const String& hostname) {
    auto it = std::remove_if(_servers.begin(), _servers.end(),
                            [&hostname](const NTPServer& s) { return s.hostname == hostname; });
    
    if (it != _servers.end()) {
        _servers.erase(it, _servers.end());
        NTP_LOG_I("Removed NTP server %s", hostname.c_str());
        return true;
    }
    
    NTP_LOG_W("Server %s not found", hostname.c_str());
    return false;
}

void NTPClient::clearServers() {
    _servers.clear();
    NTP_LOG_I("Cleared all NTP servers");
}

NTPClient::NTPServer* NTPClient::getBestServer() {
    NTPServer* best = nullptr;
    uint32_t bestScore = UINT32_MAX;
    
    for (auto& server : _servers) {
        if (!server.reachable) continue;
        
        // Score based on stratum, failures, and average RTT
        uint32_t score = (server.stratum * 1000) + 
                        (server.failureCount * 100) + 
                        server.averageRTT;
        
        if (score < bestScore) {
            bestScore = score;
            best = &server;
        }
    }
    
    return best;
}

NTPClient::SyncResult NTPClient::syncTime(uint32_t timeoutMs) {
    static SyncResult result; // Use static to avoid stack corruption on return
    result = SyncResult();    // Clear it
    
    if (!_initialized) {
        strncpy(result.error, "NTP client not initialized", sizeof(result.error) - 1);
        result.error[sizeof(result.error) - 1] = '\0';
        return result;
    }
    
    // Try best server first
    NTPServer* bestServer = getBestServer();
    if (bestServer) {
        result = syncTimeFromServer(bestServer->hostname, timeoutMs);
        if (result.success) {
            return result;
        }
    }
    
    // Try all servers in order
    for (auto& server : _servers) {
        if (!server.reachable) continue;
        
        result = syncTimeFromServer(server.hostname, timeoutMs);
        if (result.success) {
            return result;
        }
    }
    
    _syncFailures++;
    strncpy(result.error, "Failed to sync with any server", sizeof(result.error) - 1);
    result.error[sizeof(result.error) - 1] = '\0';
    return result;
}

NTPClient::SyncResult NTPClient::syncTimeFromServer(const String& hostname, uint32_t timeoutMs) {
    static SyncResult result; // Use static to avoid stack corruption on return
    result = SyncResult();    // Clear it
    result.success = false;
    strncpy(result.serverUsed, hostname.c_str(), sizeof(result.serverUsed) - 1);
    result.serverUsed[sizeof(result.serverUsed) - 1] = '\0';
    result.syncTime = 0;
    
    uint32_t startTime = millis();
    
    NTP_LOG_D("Attempting sync with %s", hostname.c_str());
    
    // Find server in list
    NTPServer* serverInfo = nullptr;
    for (auto& server : _servers) {
        if (server.hostname == hostname) {
            serverInfo = &server;
            break;
        }
    }
    
    // Send NTP request
    if (!sendNTPPacket(hostname)) {
        strncpy(result.error, "Failed to send NTP packet", sizeof(result.error) - 1);
        result.error[sizeof(result.error) - 1] = '\0';
        NTP_LOG_SYNC_FAILED(hostname.c_str(), result.error);
        if (serverInfo) {
            updateServerStats(*serverInfo, false, 0, 0);
        }
        return result;
    }
    
    // Receive response
    NTPPacket packet;
    if (!receiveNTPPacket(packet, timeoutMs)) {
        strncpy(result.error, "Timeout waiting for NTP response", sizeof(result.error) - 1);
        result.error[sizeof(result.error) - 1] = '\0';
        NTP_LOG_SYNC_FAILED(hostname.c_str(), result.error);
        if (serverInfo) {
            updateServerStats(*serverInfo, false, 0, 0);
        }
        return result;
    }
    
    // Parse response
    uint16_t rtt = millis() - startTime;
    time_t ntpTime = parseNTPPacket(packet, rtt);
    
    if (ntpTime == 0) {
        strncpy(result.error, "Invalid NTP packet received", sizeof(result.error) - 1);
        result.error[sizeof(result.error) - 1] = '\0';
        NTP_LOG_SYNC_FAILED(hostname.c_str(), result.error);
        if (serverInfo) {
            updateServerStats(*serverInfo, false, 0, 0);
        }
        return result;
    }
    
    // Calculate offset
    time_t currentTime = time(nullptr);
    int32_t offset = (int32_t)(ntpTime - currentTime) * 1000;
    
    // Apply time
    applyTimeOffset(ntpTime);
    
    // Update result
    result.success = true;
    result.offsetMs = offset;
    result.roundTripMs = rtt;
    result.stratum = packet.stratum;
    NTP_LOG_D("Setting result.syncTime to ntpTime=%ld", ntpTime);
    result.syncTime = ntpTime;
    NTP_LOG_D("Verify: result.syncTime=%ld", result.syncTime);
    
    // Update statistics
    _syncCount++;
    _lastSyncTime = ntpTime;
    _lastOffset = offset;
    
    uint32_t syncTime = millis() - startTime;
    _totalSyncTime += syncTime;
    _averageSyncTime = (float)_totalSyncTime / _syncCount;
    
    if (serverInfo) {
        updateServerStats(*serverInfo, true, offset, rtt);
        serverInfo->stratum = packet.stratum;
    }
    
    NTP_LOG_SYNC_SUCCESS(hostname.c_str(), offset);
    NTP_LOG_SERVER_STATS(hostname.c_str(), rtt, offset);
    
    // Trigger callbacks
    if (_syncCallback) {
        _syncCallback(result);
    }
    
    if (_rtcCallback) {
        _rtcCallback(ntpTime);
    }
    
    return result;
}

bool NTPClient::forceSync() {
    NTP_LOG_I("Forcing time sync");
    SyncResult result = syncTime();
    return result.success;
}

void NTPClient::setAutoSync(bool enable, uint32_t intervalSeconds) {
    _autoSyncEnabled = enable;
    _autoSyncInterval = max(intervalSeconds, MIN_SYNC_INTERVAL);
    
    NTP_LOG_I("Auto-sync %s (interval: %d seconds)", 
              enable ? "enabled" : "disabled", _autoSyncInterval);
}

time_t NTPClient::getNextSyncTime() const {
    if (!_autoSyncEnabled || _lastSyncTime == 0) {
        return 0;
    }
    return _lastSyncTime + _autoSyncInterval;
}

void NTPClient::setTimeZone(const TimeZoneConfig& config) {
    _timezone = config;
    NTP_LOG_I("Time zone set to %s (UTC%+d)", 
              config.name.c_str(), config.offsetMinutes / 60);
}

bool NTPClient::isDST() const {
    return isDST(time(nullptr));
}

bool NTPClient::isDST(time_t timestamp) const {
    if (!_timezone.useDST) return false;
    
    struct tm timeinfo;
    gmtime_r(&timestamp, &timeinfo);
    
    int year = timeinfo.tm_year + 1900;
    
    time_t dstStart = getDSTTransition(year, _timezone.dstStartMonth, 
                                      _timezone.dstStartWeek, 
                                      _timezone.dstStartDayOfWeek,
                                      _timezone.dstStartHour);
    
    time_t dstEnd = getDSTTransition(year, _timezone.dstEndMonth,
                                    _timezone.dstEndWeek,
                                    _timezone.dstEndDayOfWeek,
                                    _timezone.dstEndHour);
    
    if (dstStart < dstEnd) {
        // Northern hemisphere
        return timestamp >= dstStart && timestamp < dstEnd;
    } else {
        // Southern hemisphere
        return timestamp >= dstStart || timestamp < dstEnd;
    }
}

time_t NTPClient::getEpochTime() const {
    return time(nullptr);
}

time_t NTPClient::getLocalTime() const {
    time_t utc = time(nullptr);
    int16_t offset = _timezone.offsetMinutes;
    
    if (isDST(utc)) {
        offset += _timezone.dstOffsetMinutes;
    }
    
    return utc + (offset * 60);
}

const char* NTPClient::getFormattedTime() const {
    return getFormattedTime("%H:%M:%S");
}

const char* NTPClient::getFormattedTime(const char* format) const {
    time_t local = getLocalTime();
    
    // Check for uninitialized time (1970 epoch)
    if (local < 86400) {  // Less than 1 day since epoch
        strncpy(_formattedBuffer, "Not Synced", sizeof(_formattedBuffer) - 1);
        _formattedBuffer[sizeof(_formattedBuffer) - 1] = '\0';
        return _formattedBuffer;
    }
    
    struct tm timeinfo;
    if (!localtime_r(&local, &timeinfo)) {
        strncpy(_formattedBuffer, "Invalid Time", sizeof(_formattedBuffer) - 1);
        _formattedBuffer[sizeof(_formattedBuffer) - 1] = '\0';
        return _formattedBuffer;
    }
    
    size_t result = strftime(_formattedBuffer, sizeof(_formattedBuffer), format, &timeinfo);
    if (result == 0) {
        strncpy(_formattedBuffer, "Format Error", sizeof(_formattedBuffer) - 1);
        _formattedBuffer[sizeof(_formattedBuffer) - 1] = '\0';
    }
    
    return _formattedBuffer;
}

const char* NTPClient::getFormattedDate() const {
    return getFormattedTime("%Y-%m-%d");
}

const char* NTPClient::getFormattedDateTime() const {
    return getFormattedTime("%Y-%m-%d %H:%M:%S");
}

void NTPClient::setEpochTime(time_t epoch) {
    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    
    String timeStr = epochToString(epoch);
    NTP_LOG_I("Time set manually to %s", timeStr.c_str());
    
    if (_timeChangeCallback) {
        _timeChangeCallback(time(nullptr), epoch);
    }
}

void NTPClient::adjustTime(int32_t offsetSeconds) {
    time_t current = time(nullptr);
    setEpochTime(current + offsetSeconds);
}

void NTPClient::syncToRTC() {
    if (_rtcCallback) {
        _rtcCallback(time(nullptr));
        NTP_LOG_I("Time synced to RTC");
    }
}

void NTPClient::printDiagnostics() {
    NTP_LOG_I("=== NTP Client Diagnostics ===");
    NTP_LOG_I("Status: %s", _initialized ? "Initialized" : "Not initialized");
    NTP_LOG_I("Auto-sync: %s (interval: %ds)", 
              _autoSyncEnabled ? "ON" : "OFF", _autoSyncInterval);
    NTP_LOG_I("Current time: %s", getFormattedDateTime());
    NTP_LOG_I("Time zone: %s (UTC%+d)", 
              _timezone.name.c_str(), _timezone.offsetMinutes / 60);
    NTP_LOG_I("DST: %s", isDST() ? "Active" : "Inactive");
    String lastSyncStr = _lastSyncTime ? epochToString(_lastSyncTime) : "Never";
    NTP_LOG_I("Last sync: %s", lastSyncStr.c_str());
    NTP_LOG_I("Last offset: %ldms", _lastOffset);
    NTP_LOG_I("Sync count: %d (failures: %d)", _syncCount, _syncFailures);
    NTP_LOG_I("Average sync time: %.1fms", _averageSyncTime);
    
    NTP_LOG_I("\nServers (%d):", _servers.size());
    for (const auto& server : _servers) {
        NTP_LOG_I("  %s:%d - Stratum %d, RTT %dms, Offset %ldms, %s",
                  server.hostname.c_str(), server.port,
                  server.stratum, server.averageRTT, server.averageOffset,
                  server.reachable ? "OK" : "UNREACHABLE");
    }
    
    NTP_LOG_I("==============================");
}

void NTPClient::resetStatistics() {
    _syncCount = 0;
    _syncFailures = 0;
    _averageSyncTime = 0;
    _totalSyncTime = 0;
    
    for (auto& server : _servers) {
        server.failureCount = 0;
        server.averageOffset = 0;
        server.averageRTT = 0;
        server.reachable = true;
    }
    
    NTP_LOG_I("Statistics reset");
}

void NTPClient::process() {
    if (!_initialized || !_autoSyncEnabled) return;
    
    time_t now = time(nullptr);
    
    // Check if it's time to sync
    if (_lastSyncTime == 0 || (now - _lastSyncTime) >= _autoSyncInterval) {
        NTP_LOG_D("Auto-sync triggered");
        (void)syncTime();
    }
}

bool NTPClient::sendNTPPacket(const String& address) {
    NTPPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    // Initialize values needed for NTP request
    // li = 0, vn = 3, mode = 3 (client)
    packet.li_vn_mode = 0b00100011;
    
    // Current time as originate timestamp
    time_t now = time(nullptr);
    uint32_t origTime = now + NTP_TIMESTAMP_DELTA;
    packet.origTm_s = htonl(origTime);
    
    NTP_LOG_I("Sending NTP request to %s", address.c_str());
    NTP_LOG_I("Origin timestamp: %lu (0x%08X), current system time: %ld", 
              origTime, origTime, now);
    
    // Send packet
    if (_udp.beginPacket(address.c_str(), DEFAULT_NTP_PORT) != 1) {
        NTP_LOG_E("Failed to begin UDP packet to %s", address.c_str());
        return false;
    }
    
    _udp.write((uint8_t*)&packet, sizeof(packet));
    
    if (_udp.endPacket() != 1) {
        NTP_LOG_E("Failed to send UDP packet to %s", address.c_str());
        return false;
    }
    
    NTP_LOG_V("NTP packet sent to %s", address.c_str());
    return true;
}

bool NTPClient::receiveNTPPacket(NTPPacket& packet, uint32_t timeoutMs) {
    uint32_t startTime = millis();
    
    while ((millis() - startTime) < timeoutMs) {
        int packetSize = _udp.parsePacket();
        
        if (packetSize >= sizeof(NTPPacket)) {
            _udp.read((uint8_t*)&packet, sizeof(packet));
            NTP_LOG_V("NTP packet received (size: %d)", packetSize);
            
            // Debug: Log raw transmit timestamp bytes
            #ifdef NTP_DEBUG
            uint8_t* txBytes = (uint8_t*)&packet.txTm_s;
            NTP_LOG_V("Raw txTm_s bytes: %02X %02X %02X %02X", 
                      txBytes[0], txBytes[1], txBytes[2], txBytes[3]);
            #endif
            
            return true;
        }
        
        // Allow caller to yield control (e.g., for watchdog feeding)
        if (_yieldCallback) {
            _yieldCallback();
        }
        
        // Small delay to prevent tight loop
        delay(1);
        yield();
    }
    
    return false;
}

time_t NTPClient::parseNTPPacket(const NTPPacket& packet, uint16_t& rtt) {
    // Extract transmit timestamp
    uint32_t txTm_s = ntohl(packet.txTm_s);
    
    // Enhanced debug logging - log all timestamps
    #ifdef NTP_DEBUG
    uint32_t txTm_f = ntohl(packet.txTm_f);
    NTP_LOG_V("=== NTP Packet Debug ===");
    NTP_LOG_V("Stratum: %d, Mode: %d, Version: %d", 
              packet.stratum, packet.li_vn_mode & 0x07, (packet.li_vn_mode >> 3) & 0x07);
    NTP_LOG_V("Reference ID: 0x%08X", ntohl(packet.refId));
    NTP_LOG_V("Reference time: %lu.%lu", ntohl(packet.refTm_s), ntohl(packet.refTm_f));
    NTP_LOG_V("Origin time: %lu.%lu", ntohl(packet.origTm_s), ntohl(packet.origTm_f));
    NTP_LOG_V("Transmit time: %lu.%lu (0x%08X)", txTm_s, txTm_f, txTm_s);
    NTP_LOG_V("NTP_TIMESTAMP_DELTA: %lu", NTP_TIMESTAMP_DELTA);
    #endif
    
    // Validate NTP timestamp
    // NTP timestamps should be > 3.5 billion for dates after year 2000
    // If timestamp is < 1 billion, it's likely uptime instead of NTP time
    if (txTm_s < 1000000000UL) {
        NTP_LOG_E("INVALID NTP timestamp: %lu - server is returning uptime instead of NTP time!", txTm_s);
        NTP_LOG_E("This server is not configured correctly as an NTP server");
        NTP_LOG_E("Expected range: > 3,500,000,000 (year 2000+), got: %lu", txTm_s);
        return 0;  // Invalid time
    }
    
    // Convert NTP time to Unix time
    NTP_LOG_V("Before conversion: txTm_s=%lu, NTP_TIMESTAMP_DELTA=%lu", txTm_s, NTP_TIMESTAMP_DELTA);
    // Cast to ensure proper 32-bit arithmetic before assigning to time_t
    uint32_t unixTime32 = txTm_s - NTP_TIMESTAMP_DELTA;
    time_t ntpTime = (time_t)unixTime32;
    NTP_LOG_V("After conversion: unixTime32=%lu, ntpTime=%ld (as time_t)", unixTime32, ntpTime);
    
    // More debug
    NTP_LOG_I("Calculated Unix epoch: %ld (should be ~1,754,000,000 for 2025)", ntpTime);
    
    // Additional validation - Unix epoch should be reasonable
    // Epoch < 946684800 is before year 2000, likely invalid
    // Epoch > 2147483647 is after year 2038, likely invalid (32-bit overflow)
    if (ntpTime < 946684800L || ntpTime > 2147483647L) {
        NTP_LOG_E("Calculated epoch %ld is out of valid range (2000-2038)", ntpTime);
        return 0;  // Invalid time
    }
    
    // Adjust for network delay (simple symmetric assumption)
    ntpTime += (rtt / 2000);  // Half RTT in seconds
    
    NTP_LOG_V("NTP time: %ld, Stratum: %d", ntpTime, packet.stratum);
    
    return ntpTime;
}

void NTPClient::updateServerStats(NTPServer& server, bool success, int32_t offset, uint16_t rtt) {
    if (success) {
        server.lastSuccessTime = time(nullptr);
        server.failureCount = 0;
        
        // Update running averages (exponential moving average)
        if (server.averageOffset == 0) {
            server.averageOffset = offset;
            server.averageRTT = rtt;
        } else {
            server.averageOffset = (int32_t)((1.0f - OFFSET_FILTER_ALPHA) * server.averageOffset + 
                                            OFFSET_FILTER_ALPHA * offset);
            server.averageRTT = (uint16_t)((1.0f - OFFSET_FILTER_ALPHA) * server.averageRTT + 
                                          OFFSET_FILTER_ALPHA * rtt);
        }
    } else {
        server.failureCount++;
        
        // Mark as unreachable after too many failures
        if (server.failureCount >= MAX_RETRY_COUNT) {
            server.reachable = false;
            NTP_LOG_W("Server %s marked as unreachable", server.hostname.c_str());
        }
    }
}

time_t NTPClient::getDSTTransition(int year, uint8_t month, uint8_t week, 
                                   uint8_t dayOfWeek, uint8_t hour) const {
    struct tm timeinfo = {0};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = 1;
    timeinfo.tm_hour = hour;
    
    time_t firstDay = mktime(&timeinfo);
    struct tm* firstDayTm = gmtime(&firstDay);
    int firstDayOfWeek = firstDayTm->tm_wday;
    
    int daysUntilTarget = (dayOfWeek - firstDayOfWeek + 7) % 7;
    int targetDay = 1 + daysUntilTarget + (week - 1) * 7;
    
    // Handle "last" week of month
    if (week == 5) {
        int daysInMon = daysInMonth(month, year);
        while (targetDay > daysInMon) {
            targetDay -= 7;
        }
    }
    
    timeinfo.tm_mday = targetDay;
    return mktime(&timeinfo);
}

void NTPClient::applyTimeOffset(time_t newTime) {
    time_t oldTime = time(nullptr);
    
    struct timeval tv;
    tv.tv_sec = newTime;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    
    if (_timeChangeCallback) {
        _timeChangeCallback(oldTime, newTime);
    }
}

// Static utility methods
String NTPClient::epochToString(time_t epoch, const char* format) {
    struct tm timeinfo;
    localtime_r(&epoch, &timeinfo);
    
    char buffer[80];
    strftime(buffer, sizeof(buffer), format, &timeinfo);
    
    return String(buffer);
}

time_t NTPClient::makeTime(int year, int month, int day, 
                          int hour, int minute, int second) {
    struct tm timeinfo = {0};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    
    return mktime(&timeinfo);
}

bool NTPClient::isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t NTPClient::daysInMonth(int month, int year) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    
    return days[month - 1];
}

// Time zone presets
NTPClient::TimeZoneConfig NTPClient::getTimeZoneEST() {
    return {
        -300,      // UTC-5 hours
        "EST",
        true,      // Use DST
        2, 3, 0, 2,   // Second Sunday in March at 2:00 AM
        1, 11, 0, 2,  // First Sunday in November at 2:00 AM
        60         // +1 hour during DST
    };
}

NTPClient::TimeZoneConfig NTPClient::getTimeZonePST() {
    return {
        -480,      // UTC-8 hours
        "PST",
        true,      // Use DST
        2, 3, 0, 2,   // Second Sunday in March at 2:00 AM
        1, 11, 0, 2,  // First Sunday in November at 2:00 AM
        60         // +1 hour during DST
    };
}

NTPClient::TimeZoneConfig NTPClient::getTimeZoneCET() {
    return {
        60,        // UTC+1 hour
        "CET",
        true,      // Use DST
        5, 3, 0, 2,   // Last Sunday in March at 2:00 AM
        5, 10, 0, 3,  // Last Sunday in October at 3:00 AM
        60         // +1 hour during DST
    };
}

NTPClient::TimeZoneConfig NTPClient::getTimeZoneUTC() {
    return {
        0,         // No offset
        "UTC",
        false,     // No DST
        0, 0, 0, 0,
        0, 0, 0, 0,
        0
    };
}