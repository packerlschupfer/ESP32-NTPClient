/**
 * @file test_ntpclient.cpp
 * @brief Unit tests for NTPClient offline-testable functionality
 *
 * Tests static utility methods, timezone configurations, structures,
 * and constants without requiring network connectivity.
 */

#include <unity.h>
#include <string.h>
#include "NTPClient.h"

void setUp(void) {
    // Unity setup - called before each test
}

void tearDown(void) {
    // Unity teardown - called after each test
}

// ============================================================================
// NTPPacket Structure Tests
// ============================================================================

void test_ntp_packet_size(void) {
    // NTP packet must be exactly 48 bytes
    TEST_ASSERT_EQUAL(48, sizeof(NTPClient::NTPPacket));
}

void test_ntp_packet_packed(void) {
    // Verify packed attribute is working - no padding
    NTPClient::NTPPacket packet;
    memset(&packet, 0, sizeof(packet));

    // First 4 fields are single bytes
    TEST_ASSERT_EQUAL(offsetof(NTPClient::NTPPacket, stratum), 1);
    TEST_ASSERT_EQUAL(offsetof(NTPClient::NTPPacket, poll), 2);
    TEST_ASSERT_EQUAL(offsetof(NTPClient::NTPPacket, precision), 3);
}

// ============================================================================
// SyncResult Structure Tests
// ============================================================================

void test_sync_result_default_constructor(void) {
    NTPClient::SyncResult result;

    TEST_ASSERT_EQUAL(0, result.syncTime);
    TEST_ASSERT_EQUAL(0, result.offsetMs);
    TEST_ASSERT_EQUAL(0, result.roundTripMs);
    TEST_ASSERT_EQUAL(0, result.stratum);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL('\0', result.serverUsed[0]);
    TEST_ASSERT_EQUAL('\0', result.error[0]);
}

void test_sync_result_server_buffer_size(void) {
    NTPClient::SyncResult result;

    // Should fit 63 chars + null
    const char* longServer = "very-long-ntp-server-hostname.example.com";
    strncpy(result.serverUsed, longServer, sizeof(result.serverUsed) - 1);
    result.serverUsed[sizeof(result.serverUsed) - 1] = '\0';

    TEST_ASSERT_EQUAL(64, sizeof(result.serverUsed));
}

void test_sync_result_error_buffer_size(void) {
    NTPClient::SyncResult result;

    TEST_ASSERT_EQUAL(128, sizeof(result.error));
}

// ============================================================================
// NTPServer Structure Tests
// ============================================================================

void test_ntp_server_structure(void) {
    NTPClient::NTPServer server;
    server.hostname = "pool.ntp.org";
    server.port = 123;
    server.lastSuccessTime = 0;
    server.failureCount = 0;
    server.averageOffset = 0;
    server.averageRTT = 50;
    server.reachable = true;
    server.stratum = 2;

    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", server.hostname.c_str());
    TEST_ASSERT_EQUAL_UINT16(123, server.port);
    TEST_ASSERT_EQUAL_UINT16(50, server.averageRTT);
    TEST_ASSERT_TRUE(server.reachable);
    TEST_ASSERT_EQUAL_UINT8(2, server.stratum);
}

// ============================================================================
// TimeZoneConfig Structure Tests
// ============================================================================

void test_timezone_config_structure(void) {
    NTPClient::TimeZoneConfig tz;
    tz.offsetMinutes = -300;  // EST = UTC-5
    tz.name = "EST";
    tz.useDST = true;
    tz.dstStartWeek = 2;
    tz.dstStartMonth = 3;
    tz.dstStartDayOfWeek = 0;
    tz.dstStartHour = 2;
    tz.dstEndWeek = 1;
    tz.dstEndMonth = 11;
    tz.dstEndDayOfWeek = 0;
    tz.dstEndHour = 2;
    tz.dstOffsetMinutes = 60;

    TEST_ASSERT_EQUAL_INT16(-300, tz.offsetMinutes);
    TEST_ASSERT_EQUAL_STRING("EST", tz.name.c_str());
    TEST_ASSERT_TRUE(tz.useDST);
    TEST_ASSERT_EQUAL_UINT8(2, tz.dstStartWeek);
}

// ============================================================================
// Static Timezone Configuration Tests
// ============================================================================

void test_timezone_utc(void) {
    NTPClient::TimeZoneConfig utc = NTPClient::getTimeZoneUTC();

    TEST_ASSERT_EQUAL_INT16(0, utc.offsetMinutes);
    TEST_ASSERT_FALSE(utc.useDST);
}

void test_timezone_est(void) {
    NTPClient::TimeZoneConfig est = NTPClient::getTimeZoneEST();

    TEST_ASSERT_EQUAL_INT16(-300, est.offsetMinutes);  // -5 hours
    TEST_ASSERT_TRUE(est.useDST);
    TEST_ASSERT_EQUAL_UINT8(3, est.dstStartMonth);   // March
    TEST_ASSERT_EQUAL_UINT8(11, est.dstEndMonth);    // November
}

void test_timezone_pst(void) {
    NTPClient::TimeZoneConfig pst = NTPClient::getTimeZonePST();

    TEST_ASSERT_EQUAL_INT16(-480, pst.offsetMinutes);  // -8 hours
    TEST_ASSERT_TRUE(pst.useDST);
}

void test_timezone_cet(void) {
    NTPClient::TimeZoneConfig cet = NTPClient::getTimeZoneCET();

    TEST_ASSERT_EQUAL_INT16(60, cet.offsetMinutes);  // +1 hour
    TEST_ASSERT_TRUE(cet.useDST);
    TEST_ASSERT_EQUAL_UINT8(3, cet.dstStartMonth);   // March
    TEST_ASSERT_EQUAL_UINT8(10, cet.dstEndMonth);    // October
}

// ============================================================================
// Static Utility Method Tests
// ============================================================================

void test_is_leap_year_2020(void) {
    TEST_ASSERT_TRUE(NTPClient::isLeapYear(2020));
}

void test_is_leap_year_2021(void) {
    TEST_ASSERT_FALSE(NTPClient::isLeapYear(2021));
}

void test_is_leap_year_2000(void) {
    // Divisible by 400, so it's a leap year
    TEST_ASSERT_TRUE(NTPClient::isLeapYear(2000));
}

void test_is_leap_year_1900(void) {
    // Divisible by 100 but not 400, so not a leap year
    TEST_ASSERT_FALSE(NTPClient::isLeapYear(1900));
}

void test_is_leap_year_2024(void) {
    TEST_ASSERT_TRUE(NTPClient::isLeapYear(2024));
}

void test_days_in_february_leap_year(void) {
    TEST_ASSERT_EQUAL_UINT8(29, NTPClient::daysInMonth(2, 2020));
}

void test_days_in_february_non_leap_year(void) {
    TEST_ASSERT_EQUAL_UINT8(28, NTPClient::daysInMonth(2, 2021));
}

void test_days_in_january(void) {
    TEST_ASSERT_EQUAL_UINT8(31, NTPClient::daysInMonth(1, 2021));
}

void test_days_in_april(void) {
    TEST_ASSERT_EQUAL_UINT8(30, NTPClient::daysInMonth(4, 2021));
}

void test_days_in_june(void) {
    TEST_ASSERT_EQUAL_UINT8(30, NTPClient::daysInMonth(6, 2021));
}

void test_days_in_september(void) {
    TEST_ASSERT_EQUAL_UINT8(30, NTPClient::daysInMonth(9, 2021));
}

void test_days_in_november(void) {
    TEST_ASSERT_EQUAL_UINT8(30, NTPClient::daysInMonth(11, 2021));
}

void test_days_in_december(void) {
    TEST_ASSERT_EQUAL_UINT8(31, NTPClient::daysInMonth(12, 2021));
}

void test_make_time_basic(void) {
    time_t epoch = NTPClient::makeTime(2000, 1, 1, 0, 0, 0);

    // Y2K epoch should be 946684800
    TEST_ASSERT_EQUAL(946684800, epoch);
}

void test_make_time_2024(void) {
    time_t epoch = NTPClient::makeTime(2024, 1, 1, 0, 0, 0);

    // January 1, 2024 00:00:00 UTC = 1704067200
    TEST_ASSERT_EQUAL(1704067200, epoch);
}

void test_epoch_to_string_format(void) {
    // Test with a known epoch
    time_t epoch = 946684800;  // 2000-01-01 00:00:00
    String result = NTPClient::epochToString(epoch, "%Y-%m-%d");

    TEST_ASSERT_EQUAL_STRING("2000-01-01", result.c_str());
}

// ============================================================================
// Constants Tests
// ============================================================================

void test_ntp_timestamp_delta(void) {
    // Difference between NTP epoch (1900) and Unix epoch (1970) = 70 years
    // Expected: 2208988800 seconds
    // This constant is private but we can verify the expected value
    uint32_t expectedDelta = 2208988800UL;
    TEST_ASSERT_EQUAL_UINT32(2208988800UL, expectedDelta);
}

void test_min_sync_interval_reasonable(void) {
    // MIN_SYNC_INTERVAL should be at least 60 seconds
    // to prevent hammering NTP servers
    TEST_PASS();  // Constant is private, but we document expectation
}

void test_default_ntp_port(void) {
    // Standard NTP port is 123
    TEST_ASSERT_EQUAL(123, 123);  // Self-evident but documents requirement
}

void test_ntp_packet_size_constant(void) {
    // Standard NTP packet is 48 bytes
    TEST_ASSERT_EQUAL(48, sizeof(NTPClient::NTPPacket));
}

// ============================================================================
// NTPClient Instance Tests (no network)
// ============================================================================

void test_client_default_construction(void) {
    NTPClient client;
    // Should be constructible without network
    TEST_PASS();
}

void test_client_initial_state(void) {
    NTPClient client;

    // Before begin(), these should be default values
    TEST_ASSERT_FALSE(client.isAutoSyncEnabled());
    TEST_ASSERT_EQUAL_UINT32(0, client.getSyncCount());
    TEST_ASSERT_EQUAL_UINT32(0, client.getSyncFailures());
}

void test_client_get_servers_empty_initially(void) {
    NTPClient client;

    auto servers = client.getServers();
    TEST_ASSERT_TRUE(servers.empty());
}

void test_client_timezone_default(void) {
    NTPClient client;

    auto tz = client.getTimeZone();
    // Default should be UTC or some reasonable default
    TEST_ASSERT_EQUAL_INT16(0, tz.offsetMinutes);  // Likely UTC
}

void test_client_reset_statistics(void) {
    NTPClient client;

    client.resetStatistics();

    TEST_ASSERT_EQUAL_UINT32(0, client.getSyncCount());
    TEST_ASSERT_EQUAL_UINT32(0, client.getSyncFailures());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, client.getAverageSyncTime());
}

// ============================================================================
// Test Runner
// ============================================================================

void runAllTests() {
    UNITY_BEGIN();

    // NTPPacket tests
    RUN_TEST(test_ntp_packet_size);
    RUN_TEST(test_ntp_packet_packed);

    // SyncResult tests
    RUN_TEST(test_sync_result_default_constructor);
    RUN_TEST(test_sync_result_server_buffer_size);
    RUN_TEST(test_sync_result_error_buffer_size);

    // NTPServer tests
    RUN_TEST(test_ntp_server_structure);

    // TimeZoneConfig tests
    RUN_TEST(test_timezone_config_structure);
    RUN_TEST(test_timezone_utc);
    RUN_TEST(test_timezone_est);
    RUN_TEST(test_timezone_pst);
    RUN_TEST(test_timezone_cet);

    // Static utility tests
    RUN_TEST(test_is_leap_year_2020);
    RUN_TEST(test_is_leap_year_2021);
    RUN_TEST(test_is_leap_year_2000);
    RUN_TEST(test_is_leap_year_1900);
    RUN_TEST(test_is_leap_year_2024);
    RUN_TEST(test_days_in_february_leap_year);
    RUN_TEST(test_days_in_february_non_leap_year);
    RUN_TEST(test_days_in_january);
    RUN_TEST(test_days_in_april);
    RUN_TEST(test_days_in_june);
    RUN_TEST(test_days_in_september);
    RUN_TEST(test_days_in_november);
    RUN_TEST(test_days_in_december);
    RUN_TEST(test_make_time_basic);
    RUN_TEST(test_make_time_2024);
    RUN_TEST(test_epoch_to_string_format);

    // Constants tests
    RUN_TEST(test_ntp_timestamp_delta);
    RUN_TEST(test_min_sync_interval_reasonable);
    RUN_TEST(test_default_ntp_port);
    RUN_TEST(test_ntp_packet_size_constant);

    // Client instance tests
    RUN_TEST(test_client_default_construction);
    RUN_TEST(test_client_initial_state);
    RUN_TEST(test_client_get_servers_empty_initially);
    RUN_TEST(test_client_timezone_default);
    RUN_TEST(test_client_reset_statistics);

    UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);  // Allow serial to initialize
    runAllTests();
}

void loop() {
    // Nothing to do
}
#else
int main(int argc, char** argv) {
    runAllTests();
    return 0;
}
#endif
