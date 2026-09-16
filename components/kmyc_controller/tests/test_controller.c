#include "kmyc_controller.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Standalone oracle: no firmware headers or hardware are used. */
static uint8_t oracle_crc8(const uint8_t *data, size_t length)
{
    unsigned remainder = 0;
    for (size_t i = 0; i < length; ++i) {
        remainder ^= (unsigned)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            remainder <<= 1;
            if (remainder & 0x10000u) {
                remainder ^= 0x10700u;
            }
        }
    }
    return (uint8_t)(remainder >> 8);
}

static void put_u32(uint8_t *data, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) {
        data[i] = (uint8_t)(value >> (8u * i));
    }
}

static uint8_t identity[48] = {
    'K', 'M', 'Y', 'C', 1, 0, 0, 1, 2, 1, 2, 1, 0x3f, 0, 0, 0,
    0x78, 0x56, 0x34, 0x12, 2, 3, 8, 0xff, 5, 0x21, 2, 128,
    0xc0, 0x5d, 1, 0, 5, 0, 0, 0, 101, 0, 0xff, 0xff
};

static void test_crc_and_identity(void)
{
    const uint8_t vector[] = "123456789";
    assert(kmyc_controller_crc8(vector, 9) == 0xf4);
    assert(oracle_crc8(vector, 9) == 0xf4);
    assert(kmyc_controller_crc32(vector, 9) == UINT32_C(0xcbf43926));
    assert(kmyc_controller_crc8(NULL, 0) == 0);
    assert(kmyc_controller_crc32(NULL, 0) == 0);
    kmyc_controller_info_t info;
    assert(kmyc_controller_decode_info(identity, &info) == 0);
    assert(info.protocol_major == 1 && info.protocol_minor == 0);
    assert(info.firmware_major == 0 && info.firmware_minor == 1 && info.firmware_patch == 2);
    assert(info.hardware_major == 1 && info.hardware_minor == 2 && info.device_type == 1);
    assert(info.capabilities == 0x3f && info.uptime_ms == UINT32_C(0x12345678));
    assert(info.reset_cause == 2 && info.status == 3 && info.last_error == 8 && info.last_sequence == 255);
    assert(info.gpio_latch == 5 && info.gpio_modes == 0x21 && info.gpio_inputs_raw == 2);
    assert(info.pwm_duty == 128 && info.pwm_frequency_hz == 24000 && info.pwm_state == 1);
    assert(info.descriptor_source == 0 && info.descriptor_generation == 5 && info.descriptor_length == 101);
    assert(info.i2c_error_count == 65535);
    assert(kmyc_controller_is_supported(&info, 0x3f));
    assert(!kmyc_controller_is_supported(&info, 0x40));
    info.protocol_minor = 10;
    info.firmware_patch = 99;
    info.hardware_minor = 9;
    assert(kmyc_controller_is_supported(&info, 0));
    info.protocol_major = 2;
    assert(!kmyc_controller_is_supported(&info, 0));
    kmyc_controller_info_t saved = info;
    uint8_t bad[48];
    memcpy(bad, identity, 48);
    bad[0] ^= 1;
    assert(kmyc_controller_decode_info(bad, &info) == KMYC_CONTROLLER_ERR_PROTOCOL);
    assert(memcmp(&saved, &info, sizeof(info)) == 0);
}

static void test_frames(void)
{
    uint8_t frame[16];
    kmyc_controller_state_t state = {7, 5, 7, 0x21, true, 173};
    assert(kmyc_controller_encode_control(0x7b, KMYC_CONTROLLER_APPLY_STATE, &state, frame) == 0);
    const uint8_t expected[15] = {0xa5, 1, 0x7b, 1, 7, 5, 7, 0x21, 1, 173, 0, 0, 0, 0, 0};
    assert(memcmp(frame, expected, 15) == 0 && frame[15] == oracle_crc8(expected, 15));
    for (unsigned mode = 0; mode < 256; ++mode) {
        state.gpio_modes = (uint8_t)mode;
        bool legal = mode < 64 && (mode & 3) != 3 && ((mode >> 2) & 3) != 3 && ((mode >> 4) & 3) != 3;
        assert((kmyc_controller_encode_control(0, KMYC_CONTROLLER_APPLY_STATE, &state, frame) == 0) == legal);
    }
    state.gpio_modes = 0;
    state.gpio_update_mask = 8;
    uint8_t saved[16];
    memcpy(saved, frame, 16);
    assert(kmyc_controller_encode_control(0, KMYC_CONTROLLER_APPLY_STATE, &state, frame) == KMYC_CONTROLLER_ERR_ARGUMENT);
    assert(memcmp(saved, frame, 16) == 0);
    for (unsigned opcode = 2; opcode <= 4; ++opcode) {
        assert(kmyc_controller_encode_control(255, (kmyc_controller_opcode_t)opcode, NULL, frame) == 0);
        assert(frame[2] == 255 && frame[3] == opcode);
        for (unsigned i = 4; i < 15; ++i) {
            assert(frame[i] == 0);
        }
    }
    uint8_t response[16] = {0x5a, 1, 0x7b, 0, 5, 0x21, 2, 1, 173, 3, 8};
    response[15] = oracle_crc8(response, 15);
    kmyc_controller_response_t decoded;
    assert(kmyc_controller_decode_response(response, &decoded) == 0);
    assert(decoded.sequence == 0x7b && decoded.result == 0 && decoded.last_error == 8);
    assert(decoded.gpio_latch == 5 && decoded.gpio_modes == 0x21 && decoded.gpio_inputs_raw == 2);
    assert(decoded.pwm_state == 1 && decoded.pwm_duty == 173 && decoded.status == 3);
    kmyc_controller_response_t prior = decoded;
    for (unsigned byte = 0; byte < 16; ++byte) {
        response[byte] ^= 0x80;
        assert(kmyc_controller_decode_response(response, &decoded) < 0);
        assert(memcmp(&decoded, &prior, sizeof(decoded)) == 0);
        response[byte] ^= 0x80;
    }
}

static void seal_descriptor(uint8_t page[128], uint16_t length)
{
    memcpy(page, "KDSC", 4);
    page[4] = 1;
    page[5] = 0;
    page[6] = (uint8_t)length;
    page[7] = (uint8_t)(length >> 8);
    put_u32(page + 8, 5);
    put_u32(page + 12, kmyc_controller_crc32(page + 20, length));
    put_u32(page + 16, kmyc_controller_crc32(page, 16));
}

static void test_descriptor(void)
{
    uint8_t page[128];
    memset(page, 0xff, sizeof(page));
    seal_descriptor(page, 0);
    kmyc_controller_descriptor_t descriptor;
    assert(kmyc_controller_decode_descriptor(page, &descriptor) == 0);
    assert(descriptor.payload_length == 0 && descriptor.payload_crc32 == 0 && descriptor.generation == 5);
    size_t cursor = 0;
    kmyc_controller_tlv_t tlv;
    assert(kmyc_controller_tlv_next(&descriptor, &cursor, &tlv) == KMYC_CONTROLLER_TLV_END);
    page[20] = 0xe9; /* Unknown TLV is valid, including maximum-length data. */
    page[21] = 106;
    for (unsigned i = 22; i < 128; ++i) {
        page[i] = (uint8_t)i;
    }
    seal_descriptor(page, 108);
    assert(kmyc_controller_decode_descriptor(page, &descriptor) == 0);
    assert(kmyc_controller_tlv_next(&descriptor, &cursor, &tlv) == 0);
    assert(tlv.type == 0xe9 && tlv.length == 106 && tlv.value[105] == 127 && cursor == 108);
    assert(kmyc_controller_tlv_next(&descriptor, &cursor, &tlv) == KMYC_CONTROLLER_TLV_END);
    kmyc_controller_descriptor_t saved = descriptor;
    for (unsigned byte = 0; byte < 128; ++byte) {
        page[byte] ^= 0x80;
        assert(kmyc_controller_decode_descriptor(page, &descriptor) < 0);
        assert(memcmp(&descriptor, &saved, sizeof(descriptor)) == 0);
        page[byte] ^= 0x80;
    }
    page[21] = 107; /* CRC-valid payload is NOT necessarily valid TLV. */
    seal_descriptor(page, 108);
    assert(kmyc_controller_decode_descriptor(page, &descriptor) == 0);
    cursor = 0;
    assert(kmyc_controller_tlv_next(&descriptor, &cursor, &tlv) == KMYC_CONTROLLER_ERR_TLV && cursor == 0);
    page[20] = 1;
    seal_descriptor(page, 1);
    assert(kmyc_controller_decode_descriptor(page, &descriptor) == 0);
    assert(kmyc_controller_tlv_next(&descriptor, &cursor, &tlv) == KMYC_CONTROLLER_ERR_TLV);
    seal_descriptor(page, 0);
    page[5] = 1;
    put_u32(page + 16, kmyc_controller_crc32(page, 16));
    assert(kmyc_controller_decode_descriptor(page, &descriptor) == 0);
    page[127] ^= 1; /* Padding is outside both CRC ranges. */
    assert(kmyc_controller_decode_descriptor(page, &descriptor) == 0);
    page[6] = 109;
    assert(kmyc_controller_decode_descriptor(page, &descriptor) == KMYC_CONTROLLER_ERR_PROTOCOL);
}

typedef struct {
    unsigned reads, writes;
    int read_error, write_error;
    uint8_t reply[16], last_write[16];
    bool stale_sequence, corrupt_crc;
    uint8_t device_result;
} mock_t;

static int mock_read(void *context, uint8_t reg, uint8_t *data, size_t length)
{
    mock_t *mock = context;
    ++mock->reads;
    if (mock->read_error) {
        return mock->read_error;
    }
    if (reg == 0) {
        assert(length == 48);
        memcpy(data, identity, length);
    } else if (reg == 0x40) {
        assert(length == 16);
        memcpy(data, mock->reply, length);
    } else {
        assert(reg == 0x80 && length == 128);
        memset(data, 0xff, length);
        seal_descriptor(data, 0);
    }
    return 0;
}

static int mock_write(void *context, uint8_t reg, const uint8_t *data, size_t length)
{
    mock_t *mock = context;
    assert(reg == 0x30 && length == 16); /* Never a configuration write. */
    assert(data[15] == oracle_crc8(data, 15));
    ++mock->writes;
    memcpy(mock->last_write, data, 16);
    memset(mock->reply, 0, 16);
    mock->reply[0] = 0x5a;
    mock->reply[1] = 1;
    mock->reply[2] = (uint8_t)(data[2] - (mock->stale_sequence ? 1 : 0));
    mock->reply[3] = mock->device_result;
    mock->reply[9] = 3;
    mock->reply[10] = 8; /* Historical error must not override a successful result. */
    mock->reply[15] = oracle_crc8(mock->reply, 15) ^ (mock->corrupt_crc ? 1 : 0);
    return mock->write_error;
}

static void test_client(void)
{
    mock_t mock = {0};
    kmyc_controller_transport_t transport = {&mock, mock_read, mock_write};
    kmyc_controller_t client;
    kmyc_controller_info_t info;
    kmyc_controller_state_t state = {2, 2, 2, 4, true, 128};
    assert(kmyc_controller_init(&client, &transport) == 0);
    assert(kmyc_controller_apply(&client, &state, NULL) == KMYC_CONTROLLER_ERR_NOT_PROBED);
    assert(mock.reads == 0 && mock.writes == 0);
    assert(kmyc_controller_probe(&client, &info) == 0 && client.sequence == 255);
    assert(kmyc_controller_apply(&client, &state, NULL) == 0 && client.sequence == 0);
    assert(mock.writes == 1 && mock.reads == 2);
    assert(kmyc_controller_release_pwm(&client, NULL) == 0);
    assert(kmyc_controller_release_all(&client, NULL) == 0);
    assert(kmyc_controller_clear_diagnostics(&client, NULL) == 0);
    assert(mock.writes == 4 && mock.reads == 5);
    kmyc_controller_descriptor_t descriptor;
    assert(kmyc_controller_read_descriptor(&client, &descriptor) == 0 && descriptor.generation == 5);
    mock.stale_sequence = true;
    assert(kmyc_controller_apply(&client, &state, NULL) == KMYC_CONTROLLER_ERR_SEQUENCE);
    mock.stale_sequence = false;
    mock.corrupt_crc = true;
    assert(kmyc_controller_apply(&client, &state, NULL) == KMYC_CONTROLLER_ERR_CRC);
    mock.corrupt_crc = false;
    mock.device_result = 7;
    kmyc_controller_response_t response;
    assert(kmyc_controller_apply(&client, &state, &response) == KMYC_CONTROLLER_ERR_DEVICE && response.result == 7);
    mock.device_result = 0;
    mock.write_error = 42;
    unsigned before_writes = mock.writes, before_reads = mock.reads;
    uint8_t sequence = client.sequence;
    assert(kmyc_controller_apply(&client, &state, NULL) == KMYC_CONTROLLER_ERR_TRANSPORT);
    assert(client.last_transport_error == 42 && client.sequence == (uint8_t)(sequence + 1));
    assert(mock.writes == before_writes + 1 && mock.reads == before_reads); /* No retries. */
    assert(kmyc_controller_read_response(&client, &response) == 0 && response.sequence == client.sequence);
    mock.write_error = 0;
    mock.read_error = 43;
    assert(kmyc_controller_apply(&client, &state, NULL) == KMYC_CONTROLLER_ERR_TRANSPORT && client.last_transport_error == 43);
    mock.read_error = 0;
    client.capabilities = 0;
    before_writes = mock.writes;
    assert(kmyc_controller_apply(&client, &state, NULL) == KMYC_CONTROLLER_ERR_UNSUPPORTED);
    assert(mock.writes == before_writes);
    identity[4] = 2;
    assert(kmyc_controller_probe(&client, &info) == KMYC_CONTROLLER_ERR_UNSUPPORTED && !client.probed);
    identity[4] = 1;
}

int main(void)
{
    test_crc_and_identity();
    test_frames();
    test_descriptor();
    test_client();
    puts("kmyc_controller host tests passed (mock transport; no hardware)");
    return 0;
}
