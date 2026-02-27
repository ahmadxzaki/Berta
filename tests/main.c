#include <stdio.h>
#include "embUnit.h"
#include "message_encoding.h" 

static const uint8_t FAKE_KEY[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

// 1. Define a test case
static void test_encode_tracker_ping_pkt(void)
{
    uint8_t bufferOut[13];

    create_v1_tracker_ping_pkt(
        bufferOut,
        FAKE_KEY,
        sizeof(FAKE_KEY),
        3, // battery level,
        false, // in emergency mode,
        45345345, // tracker id
        34 // counter
    );

    printf("Encoded packet: ");
    for (int i = 0; i < 13; i++) {
        // Use "%02X" for two uppercase hex digits, with leading zeros
        printf("%02X ", bufferOut[i]);
    }

    printf("\n"); // Print a newline at the end
}

static void test_parse_tracker_pong_pkt_valid(void)
{
    // A mock 9-byte packet (Command: 0x02, Counter: 0x00000005, CMAC: Fake/Pre-calculated)
    uint8_t mock_pkt[9] = {0x02, 0x05, 0x00, 0x00, 0x00, 0x1D, 0x43, 0xEE, 0x74}; 

    uint8_t cmd_out;
    uint32_t counter_out;

    bool result = parse_v1_tracker_pong_pkt(
        mock_pkt, 
        FAKE_KEY,
        sizeof(FAKE_KEY),
        &cmd_out, 
        &counter_out
    );

    // 2. Assert the outcomes
    TEST_ASSERT_EQUAL_INT(true, result);
    TEST_ASSERT_EQUAL_INT(0x02, cmd_out);
    TEST_ASSERT_EQUAL_INT(5, counter_out);
}

// 3. Group tests into a fixture
Test* tests_comms_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        // Pass exactly one argument, and ensure the spelling matches the function above!
        new_TestFixture(test_parse_tracker_pong_pkt_valid),
        new_TestFixture(test_encode_tracker_ping_pkt),
    };

    EMB_UNIT_TESTCALLER(comms_tests, NULL, NULL, fixtures);
    return (Test*)&comms_tests;
}

// 4. Run the suite
int main(void)
{
    TestRunner_start();
    TestRunner_runTest(tests_comms_tests());
    TestRunner_end();

    return 0;
}