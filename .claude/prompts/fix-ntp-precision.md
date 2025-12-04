# ESP32-NTPClient - Fix Millisecond Precision Bug

## Context

The ESP32-NTPClient library is part of an 18-library ecosystem for an industrial ESP32 boiler controller. The library currently has a critical precision bug causing false drift measurements and unreliable time synchronization.

**Library Location:** `/home/mrnice/Documents/PlatformIO/git/ESP32-NTPClient`

## Bug Discovery

Production logs showed suspicious NTP drift pattern:

```
2026-01-01T03:17:04+01:00 esp32-boiler boiler[NTPTask]: Initial time sync from 192.168.20.1, offset 1000ms, RTT 5ms
2026-01-01T04:17:03+01:00 esp32-boiler boiler[NTPTask]: Time synced from 192.168.20.1, offset 0ms, RTT 5ms
2026-01-01T04:17:03+01:00 esp32-boiler boiler[NTPTask]: Clock drift: 2757ms over 3599s (765.9 ppm), avg: 2757ms
2026-01-01T05:17:03+01:00 esp32-boiler boiler[NTPTask]: Time synced from 192.168.20.1, offset 1000ms, RTT 5ms
2026-01-01T05:17:03+01:00 esp32-boiler boiler[NTPTask]: Clock drift: -168ms over 3599s (-46.7 ppm), avg: 1294ms
2026-01-01T06:17:04+01:00 esp32-boiler boiler[NTPTask]: Time synced from 192.168.20.1, offset 1000ms, RTT 5ms
2026-01-01T06:17:04+01:00 esp32-boiler boiler[NTPTask]: Clock drift: -712ms over 3600s (-197.8 ppm), avg: 625ms
2026-01-01T07:17:03+01:00 esp32-boiler boiler[NTPTask]: Time synced from 192.168.20.1, offset 0ms, RTT 5ms
2026-01-01T07:17:03+01:00 esp32-boiler boiler[NTPTask]: Clock drift: 2688ms over 3599s (746.7 ppm), avg: 1141ms
```

**Pattern Observed:**
- Offset alternates between 0ms and 1000ms in regular pattern
- Timestamps oscillate between ending at `:03` and `:04` seconds
- Calculated drift swings wildly between +765 ppm and -473 ppm
- This is NOT how real ESP32 crystal oscillator drift behaves

**Root Cause:** The library is discarding NTP fractional seconds, causing systematic measurement errors.

## The Three Bugs

### Bug 1: NTP Fractional Seconds Ignored

**File:** `src/NTPClient.cpp:532`

```cpp
time_t NTPClient::parseNTPPacket(const NTPPacket& packet, uint16_t& rtt) {
    // Extract transmit timestamp
    uint32_t txTm_s = ntohl(packet.txTm_s);  // ← Only extracts SECONDS

    // BUG: packet.txTm_f (fractional seconds) is NEVER USED!
    // uint32_t txTm_f = ntohl(packet.txTm_f);  // This line doesn't exist!
```

**Issue:** NTP timestamps are 64-bit fixed-point: 32 bits integer seconds + 32 bits fractional seconds. The fractional part provides ~232 picosecond resolution. Code only uses integer seconds, losing all sub-second precision.

### Bug 2: Offset Calculation Uses time() Instead of gettimeofday()

**File:** `src/NTPClient.cpp:228-229`

```cpp
// Calculate offset
time_t currentTime = time(nullptr);  // ← Only second precision!
int32_t offset = (int32_t)(ntpTime - currentTime) * 1000;
```

**Issue:** `time()` returns whole seconds only. Even if we extracted NTP fractions, this comparison would lose millisecond precision. Should use `gettimeofday()` which returns microseconds.

### Bug 3: settimeofday() Always Sets Microseconds to Zero

**File:** `src/NTPClient.cpp:636-642`

```cpp
void NTPClient::applyTimeOffset(time_t newTime) {
    time_t oldTime = time(nullptr);

    struct timeval tv;
    tv.tv_sec = newTime;
    tv.tv_usec = 0;  // ← Always zero! Should be set from NTP fractions
    settimeofday(&tv, nullptr);
```

**Issue:** Even though `settimeofday()` accepts microsecond precision via `tv.tv_usec`, it's always hardcoded to zero.

## Why the Alternating Pattern Occurs

1. **First sync at 03:17:04**: NTP server actual time is `03:17:04.600` (with ~600ms fractions)
   - Code ignores fractions, sets system time to exactly `03:17:04.000`
   - Reports offset = 1000ms (because server was at :04.600, system was at :03.something)

2. **One hour later at 04:17:03**: System drifts naturally over 1 hour
   - NTP server time: `04:17:04.600` (with ~600ms fractions)
   - `time(nullptr)` returns `04:17:04` (truncated to seconds)
   - Code sees 0 second difference → offset = 0ms
   - But actual offset is ~600ms! This is lost due to truncation.
   - Sets time to exactly `04:17:04.000`

3. **Next sync**:
   - NTP server time: `05:17:05.600`
   - `time(nullptr)` returns `05:17:04`
   - Code sees 1 second difference → offset = 1000ms
   - Pattern repeats

The alternation happens because NTP fractional seconds (~600ms in this case) create a sawtooth pattern when system time has no sub-second precision.

## The Fix - Implementation Plan

### 1. Add Fractional Seconds Extraction

**File:** `src/NTPClient.cpp:530-580`

```cpp
time_t NTPClient::parseNTPPacket(const NTPPacket& packet, uint16_t& rtt) {
    // Extract transmit timestamp (BOTH integer and fractional parts)
    uint32_t txTm_s = ntohl(packet.txTm_s);
    uint32_t txTm_f = ntohl(packet.txTm_f);  // ← ADD THIS

    // Convert NTP fractional seconds to microseconds
    // NTP fraction is in units of 2^-32 seconds
    // To convert to microseconds: (fraction * 1,000,000) / 2^32
    // Optimized: (fraction >> 12) * 1000000 >> 20
    uint32_t usec = (uint32_t)(((uint64_t)txTm_f * 1000000ULL) >> 32);

    // ... existing validation ...

    // Convert NTP time to Unix time (seconds only for now)
    uint32_t unixTime32 = txTm_s - NTP_TIMESTAMP_DELTA;
    time_t ntpTime = (time_t)unixTime32;

    // Return a structure with BOTH seconds and microseconds
    // OR modify function signature to return via output parameters
    // Option A: Modify parseNTPPacket to return struct { time_t sec; uint32_t usec; }
    // Option B: Add output parameter: parseNTPPacket(..., uint32_t& usecOut)
}
```

### 2. Update SyncResult Structure

**File:** `src/NTPClient.h`

Add microseconds field to `SyncResult`:

```cpp
struct SyncResult {
    bool success;
    time_t syncTime;      // Unix epoch seconds
    uint32_t syncUsec;    // ← ADD THIS - microseconds (0-999999)
    int32_t offsetMs;
    uint16_t roundTripMs;
    uint8_t stratum;
    char serverUsed[64];
    char error[128];
};
```

### 3. Fix Offset Calculation

**File:** `src/NTPClient.cpp:169-240` (in `syncTimeFromServer`)

```cpp
// Parse response (now returns both sec + usec)
uint16_t rtt = millis() - startTime;
uint32_t ntpUsec = 0;
time_t ntpTime = parseNTPPacket(packet, rtt, ntpUsec);  // Modified signature

if (ntpTime == 0) {
    // ... error handling ...
}

// Calculate offset with MICROSECOND precision
struct timeval currentTv;
gettimeofday(&currentTv, nullptr);  // ← Use gettimeofday() not time()

// Calculate offset in milliseconds (with sub-ms precision from usec)
int64_t ntpEpochUs = (int64_t)ntpTime * 1000000LL + ntpUsec;
int64_t sysEpochUs = (int64_t)currentTv.tv_sec * 1000000LL + currentTv.tv_usec;
int64_t offsetUs = ntpEpochUs - sysEpochUs;
int32_t offsetMs = (int32_t)(offsetUs / 1000LL);

// Apply time with microsecond precision
applyTimeOffset(ntpTime, ntpUsec);  // ← Modified signature

// Update result
result.success = true;
result.syncTime = ntpTime;
result.syncUsec = ntpUsec;  // ← NEW FIELD
result.offsetMs = offsetMs;
result.roundTripMs = rtt;
```

### 4. Fix applyTimeOffset

**File:** `src/NTPClient.cpp:636-647`

```cpp
void NTPClient::applyTimeOffset(time_t newTime, uint32_t newUsec) {  // ← Add usec parameter
    time_t oldTime = time(nullptr);

    struct timeval tv;
    tv.tv_sec = newTime;
    tv.tv_usec = newUsec;  // ← Set from NTP fractions, not zero!
    settimeofday(&tv, nullptr);

    if (_timeChangeCallback) {
        _timeChangeCallback(oldTime, newTime);
    }
}
```

### 5. Update Function Signatures

**File:** `src/NTPClient.h`

```cpp
class NTPClient {
private:
    // Update signatures
    time_t parseNTPPacket(const NTPPacket& packet, uint16_t& rtt, uint32_t& usecOut);
    void applyTimeOffset(time_t newTime, uint32_t newUsec);

    // ... rest of class ...
};
```

### 6. Fix Drift Calculation in Main Project

**File:** `/home/mrnice/Documents/PlatformIO/Projects/esp32-boiler-controller/src/modules/tasks/NTPTask.cpp:166-194`

The drift calculation in NTPTask.cpp should now work correctly because `result.offsetMs` will have true millisecond precision:

```cpp
// Current code (lines 179-182):
driftMs = (int32_t)(timeDiffSeconds * 1000) + (int32_t)(millisElapsed % 1000);
// Subtract the NTP offset - we want actual drift, not the correction applied
driftMs -= result.offsetMs;  // ← This will now be accurate!
```

**No changes needed** in NTPTask.cpp if the library fix is correct.

## Testing Strategy

### 1. Unit Tests (Create New File)

**File:** `test/test_ntp_precision/test_main.cpp`

```cpp
#include <unity.h>
#include "NTPClient.h"

void test_ntp_fraction_conversion() {
    // Test fractional conversion
    // NTP fraction 0x80000000 = 0.5 seconds = 500000 microseconds
    uint32_t fraction = 0x80000000;
    uint32_t usec = (uint32_t)(((uint64_t)fraction * 1000000ULL) >> 32);
    TEST_ASSERT_EQUAL_UINT32(500000, usec);

    // 0xFFFFFFFF ≈ 0.999999999 seconds ≈ 999999 microseconds
    fraction = 0xFFFFFFFF;
    usec = (uint32_t)(((uint64_t)fraction * 1000000ULL) >> 32);
    TEST_ASSERT_INT_WITHIN(1, 999999, usec);
}

void test_offset_calculation_precision() {
    // Mock scenario: NTP says 10:00:00.600, system is 10:00:00.100
    // Expected offset: +500ms

    struct timeval mockSystem = { .tv_sec = 1000000, .tv_usec = 100000 };
    time_t ntpSec = 1000000;
    uint32_t ntpUsec = 600000;

    int64_t ntpEpochUs = (int64_t)ntpSec * 1000000LL + ntpUsec;
    int64_t sysEpochUs = (int64_t)mockSystem.tv_sec * 1000000LL + mockSystem.tv_usec;
    int64_t offsetUs = ntpEpochUs - sysEpochUs;
    int32_t offsetMs = (int32_t)(offsetUs / 1000LL);

    TEST_ASSERT_EQUAL_INT32(500, offsetMs);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ntp_fraction_conversion);
    RUN_TEST(test_offset_calculation_precision);
    return UNITY_END();
}
```

### 2. Integration Test with Real NTP Server

**File:** `examples/precision_test/precision_test.ino`

```cpp
#include <NTPClient.h>
#include <WiFi.h>

NTPClient ntp;

void setup() {
    Serial.begin(921600);

    // Connect to WiFi
    WiFi.begin("SSID", "PASS");
    while (WiFi.status() != WL_CONNECTED) delay(100);

    ntp.begin();
    ntp.addServer("192.168.20.1", 123);  // Local server
    ntp.addServer("pool.ntp.org", 123);
}

void loop() {
    auto result = ntp.syncTime(5000);

    if (result.success) {
        // NEW: Check that syncUsec is populated
        Serial.printf("Sync OK: %ld.%06lu, offset=%ldms, RTT=%dms\n",
                     result.syncTime, result.syncUsec,
                     result.offsetMs, result.roundTripMs);

        // Verify system time has microseconds
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        Serial.printf("System time: %ld.%06ld\n", tv.tv_sec, tv.tv_usec);

        // PASS criteria: tv.tv_usec should NOT be zero
        if (tv.tv_usec == 0) {
            Serial.println("FAIL: System microseconds still zero!");
        }
    } else {
        Serial.printf("Sync failed: %s\n", result.error);
    }

    delay(10000);  // Test every 10 seconds
}
```

### 3. Production Validation

After deploying to boiler controller:

**Expected Results:**
- Offsets should vary smoothly (e.g., 234ms, 567ms, 123ms) instead of quantized (0ms, 1000ms)
- Drift measurements should be stable (~200-300 ppm for ESP32)
- No more wild swings between +700 ppm and -400 ppm
- Log pattern should show realistic sub-second offsets

**Validation Log Pattern:**
```
Time synced from 192.168.20.1, offset 234ms, RTT 5ms
Clock drift: 756ms over 3599s (210.0 ppm), avg: 756ms
Time synced from 192.168.20.1, offset 567ms, RTT 5ms
Clock drift: 812ms over 3600s (225.6 ppm), avg: 784ms
Time synced from 192.168.20.1, offset 123ms, RTT 5ms
Clock drift: 789ms over 3599s (219.2 ppm), avg: 785ms
```

No more 0ms/1000ms alternation!

## Files to Modify

1. **`src/NTPClient.h`**
   - Add `syncUsec` field to `SyncResult` struct
   - Update `parseNTPPacket()` signature
   - Update `applyTimeOffset()` signature

2. **`src/NTPClient.cpp`**
   - `parseNTPPacket()` (line 530): Extract `txTm_f`, convert to microseconds
   - `syncTimeFromServer()` (line 228): Use `gettimeofday()` for offset calc
   - `applyTimeOffset()` (line 641): Set `tv.tv_usec` from parameter

3. **`test/test_ntp_precision/test_main.cpp`** (NEW)
   - Create unit tests for fraction conversion
   - Test offset calculation precision

4. **`examples/precision_test/precision_test.ino`** (NEW)
   - Create integration test for real NTP sync
   - Verify microseconds are properly set

## Backward Compatibility

The changes are **backward compatible**:
- New `syncUsec` field in `SyncResult` defaults to 0 if not set
- Existing code that ignores `syncUsec` will continue to work
- Only code that reads `syncUsec` will benefit from new precision

## Related Files in Main Project

**Main project drift calculation** (no changes needed if library is fixed):
- `/home/mrnice/Documents/PlatformIO/Projects/esp32-boiler-controller/src/modules/tasks/NTPTask.cpp:166-194`

The existing drift calculation should work correctly once `result.offsetMs` has true millisecond precision.

## Success Criteria

1. **Unit tests pass**: Fraction conversion and offset calculation tests
2. **Integration test shows microseconds**: System time has non-zero `tv_usec` after sync
3. **Production logs show smooth offsets**: No more 0ms/1000ms alternation
4. **Drift measurements are stable**: Typically 200-300 ppm for ESP32, no wild swings
5. **No breaking changes**: Existing library users continue to work

## Questions to Address

1. Should we also update RTC sync to use microseconds? (DS3231 only has 1-second resolution)
2. Do we need to handle NTP leap seconds? (Currently not implemented)
3. Should we add statistics for offset jitter now that we have true precision?
4. Network RTT compensation: Currently uses `ntpTime += (rtt / 2000)` - should this be more precise?

## Additional Context

**Library Ecosystem:**
- Part of 18-library ecosystem for industrial boiler controller
- GitHub: https://github.com/packerlschupfer/ESP32-NTPClient
- Used by: ESP32 boiler controller production system
- Dependencies: Arduino WiFiUdp, ESP32 timeval/settimeofday

**Production Impact:**
- Currently running with 1-hour sync interval
- False drift measurements trigger unnecessary warnings
- Accurate drift tracking needed for long-term reliability analysis

**Development Philosophy:**
- Precision matters: Industrial control system
- Thread-safe by design
- No dynamic allocation (static buffers)
- Comprehensive logging for debugging

Please implement the fix with full millisecond precision support.
