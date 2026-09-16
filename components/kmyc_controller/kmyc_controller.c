#include "kmyc_controller.h"

#include <string.h>

static uint16_t get_u16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t get_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool modes_valid(uint8_t modes)
{
    return (modes & 0xc0u) == 0u && (modes & 3u) != 3u &&
           ((modes >> 2) & 3u) != 3u && ((modes >> 4) & 3u) != 3u;
}

uint8_t kmyc_controller_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (uint8_t)((crc & 0x80u) ? ((unsigned)crc << 1) ^ 0x07u : (unsigned)crc << 1);
        }
    }
    return crc;
}

uint32_t kmyc_controller_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0u);
        }
    }
    return crc ^ UINT32_MAX;
}

int kmyc_controller_decode_info(const uint8_t data[48], kmyc_controller_info_t *info)
{
    if (!data || !info) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    if (memcmp(data, "KMYC", 4) != 0) {
        return KMYC_CONTROLLER_ERR_PROTOCOL;
    }
    kmyc_controller_info_t value = {0};
    value.protocol_major = data[4];
    value.protocol_minor = data[5];
    value.firmware_major = data[6];
    value.firmware_minor = data[7];
    value.firmware_patch = data[8];
    value.hardware_major = data[9];
    value.hardware_minor = data[10];
    value.device_type = data[11];
    value.capabilities = get_u32(data + 12);
    value.uptime_ms = get_u32(data + 16);
    value.reset_cause = data[20];
    value.status = data[21];
    value.last_error = data[22];
    value.last_sequence = data[23];
    value.gpio_latch = data[24];
    value.gpio_modes = data[25];
    value.gpio_inputs_raw = data[26];
    value.pwm_duty = data[27];
    value.pwm_frequency_hz = get_u16(data + 28);
    value.pwm_state = data[30];
    value.descriptor_source = data[31];
    value.descriptor_generation = get_u32(data + 32);
    value.descriptor_length = get_u16(data + 36);
    value.i2c_error_count = get_u16(data + 38);
    /* Capability/status/reserved extensions are not grounds for rejecting a
     * future minor version; callers gate the major version and required bits. */
    *info = value;
    return KMYC_CONTROLLER_OK;
}

bool kmyc_controller_is_supported(const kmyc_controller_info_t *info, uint32_t required_capabilities)
{
    return info && info->protocol_major == 1u && info->device_type == 1u &&
           (info->capabilities & required_capabilities) == required_capabilities;
}

int kmyc_controller_encode_control(uint8_t sequence, kmyc_controller_opcode_t opcode,
                                   const kmyc_controller_state_t *state, uint8_t data[16])
{
    if (!data || opcode < KMYC_CONTROLLER_APPLY_STATE || opcode > KMYC_CONTROLLER_CLEAR_DIAGNOSTICS ||
        (opcode == KMYC_CONTROLLER_APPLY_STATE && !state)) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    uint8_t frame[16] = {0xa5, 1, sequence, (uint8_t)opcode};
    if (opcode == KMYC_CONTROLLER_APPLY_STATE) {
        if (((state->gpio_update_mask | state->gpio_values | state->gpio_mode_update_mask) & 0xf8u) ||
            !modes_valid(state->gpio_modes)) {
            return KMYC_CONTROLLER_ERR_ARGUMENT;
        }
        frame[4] = state->gpio_update_mask;
        frame[5] = state->gpio_values;
        frame[6] = state->gpio_mode_update_mask;
        frame[7] = state->gpio_modes;
        frame[8] = state->pwm_update ? 1u : 0u;
        frame[9] = state->pwm_duty;
    }
    frame[15] = kmyc_controller_crc8(frame, 15);
    memcpy(data, frame, sizeof(frame));
    return KMYC_CONTROLLER_OK;
}

int kmyc_controller_decode_response(const uint8_t data[16], kmyc_controller_response_t *response)
{
    if (!data || !response) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    if (data[0] != 0x5au || data[1] != 1u) {
        return KMYC_CONTROLLER_ERR_PROTOCOL;
    }
    if (kmyc_controller_crc8(data, 15) != data[15]) {
        return KMYC_CONTROLLER_ERR_CRC;
    }
    if ((data[4] & 0xf8u) || !modes_valid(data[5]) || (data[6] & 0xf8u)) {
        return KMYC_CONTROLLER_ERR_PROTOCOL;
    }
    kmyc_controller_response_t value = {
        .sequence = data[2], .result = data[3], .gpio_latch = data[4],
        .gpio_modes = data[5], .gpio_inputs_raw = data[6], .pwm_state = data[7],
        .pwm_duty = data[8], .status = data[9], .last_error = data[10]
    };
    *response = value;
    return KMYC_CONTROLLER_OK;
}

int kmyc_controller_decode_descriptor(const uint8_t data[128], kmyc_controller_descriptor_t *descriptor)
{
    if (!data || !descriptor) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    uint16_t length = get_u16(data + 6);
    if (memcmp(data, "KDSC", 4) != 0 || data[4] != 1u || (data[5] & 0xfeu) ||
        length > KMYC_CONTROLLER_PAYLOAD_MAX || ((data[5] & 1u) && length != 0u)) {
        return KMYC_CONTROLLER_ERR_PROTOCOL;
    }
    if (kmyc_controller_crc32(data, 16) != get_u32(data + 16) ||
        kmyc_controller_crc32(data + 20, length) != get_u32(data + 12)) {
        return KMYC_CONTROLLER_ERR_CRC;
    }
    kmyc_controller_descriptor_t value = {
        .format_version = data[4], .flags = data[5], .payload_length = length,
        .generation = get_u32(data + 8), .payload_crc32 = get_u32(data + 12)
    };
    memcpy(value.payload, data + 20, length);
    *descriptor = value;
    return KMYC_CONTROLLER_OK;
}

int kmyc_controller_tlv_next(const kmyc_controller_descriptor_t *descriptor, size_t *cursor,
                             kmyc_controller_tlv_t *tlv)
{
    if (!descriptor || !cursor || !tlv || descriptor->payload_length > KMYC_CONTROLLER_PAYLOAD_MAX ||
        *cursor > descriptor->payload_length) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    size_t remaining = descriptor->payload_length - *cursor;
    if (remaining == 0) {
        return KMYC_CONTROLLER_TLV_END;
    }
    if (remaining < 2 || descriptor->payload[*cursor + 1] > remaining - 2) {
        return KMYC_CONTROLLER_ERR_TLV;
    }
    tlv->type = descriptor->payload[*cursor];
    tlv->length = descriptor->payload[*cursor + 1];
    tlv->value = descriptor->payload + *cursor + 2;
    *cursor += 2u + tlv->length;
    return KMYC_CONTROLLER_OK;
}

int kmyc_controller_init(kmyc_controller_t *client, const kmyc_controller_transport_t *transport)
{
    if (!client || !transport || !transport->read || !transport->write) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    kmyc_controller_transport_t saved = *transport;
    memset(client, 0, sizeof(*client));
    client->transport = saved;
    return KMYC_CONTROLLER_OK;
}

static int read_register(kmyc_controller_t *client, uint8_t reg, uint8_t *data, size_t length)
{
    if (!client || !client->transport.read) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    client->last_transport_error = client->transport.read(client->transport.context, reg, data, length);
    return client->last_transport_error == 0 ? KMYC_CONTROLLER_OK : KMYC_CONTROLLER_ERR_TRANSPORT;
}

int kmyc_controller_read_info(kmyc_controller_t *client, kmyc_controller_info_t *info)
{
    if (!info) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    uint8_t data[48];
    int result = read_register(client, 0, data, sizeof(data));
    return result == 0 ? kmyc_controller_decode_info(data, info) : result;
}

int kmyc_controller_probe(kmyc_controller_t *client, kmyc_controller_info_t *info)
{
    if (!client || !info) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    client->probed = false;
    int result = kmyc_controller_read_info(client, info);
    if (result != 0) {
        return result;
    }
    if (!kmyc_controller_is_supported(info, 0)) {
        return KMYC_CONTROLLER_ERR_UNSUPPORTED;
    }
    client->capabilities = info->capabilities;
    client->sequence = info->last_sequence;
    client->probed = true;
    return KMYC_CONTROLLER_OK;
}

int kmyc_controller_read_response(kmyc_controller_t *client, kmyc_controller_response_t *response)
{
    if (!response) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    uint8_t data[16];
    int result = read_register(client, 0x40, data, sizeof(data));
    return result == 0 ? kmyc_controller_decode_response(data, response) : result;
}

int kmyc_controller_read_descriptor(kmyc_controller_t *client, kmyc_controller_descriptor_t *descriptor)
{
    if (!client || !descriptor) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    if (!client->probed) {
        return KMYC_CONTROLLER_ERR_NOT_PROBED;
    }
    if (!(client->capabilities & KMYC_CONTROLLER_CAP_DESCRIPTOR_TLV)) {
        return KMYC_CONTROLLER_ERR_UNSUPPORTED;
    }
    uint8_t data[128];
    int result = read_register(client, 0x80, data, sizeof(data));
    return result == 0 ? kmyc_controller_decode_descriptor(data, descriptor) : result;
}

static int command(kmyc_controller_t *client, kmyc_controller_opcode_t opcode,
                   const kmyc_controller_state_t *state, kmyc_controller_response_t *response,
                   uint32_t required_capabilities)
{
    if (!client || !client->transport.write) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    if (!client->probed) {
        return KMYC_CONTROLLER_ERR_NOT_PROBED;
    }
    if ((client->capabilities & required_capabilities) != required_capabilities) {
        return KMYC_CONTROLLER_ERR_UNSUPPORTED;
    }
    uint8_t data[16];
    uint8_t next = (uint8_t)(client->sequence + 1u);
    int result = kmyc_controller_encode_control(next, opcode, state, data);
    if (result != 0) {
        return result;
    }
    /* Retain the attempted sequence even when a transport failure leaves the
     * commit outcome unknown. The caller may inspect a fresh response, not retry. */
    client->sequence = next;
    client->last_transport_error = client->transport.write(client->transport.context, 0x30, data, sizeof(data));
    if (client->last_transport_error != 0) {
        return KMYC_CONTROLLER_ERR_TRANSPORT;
    }
    kmyc_controller_response_t received;
    result = kmyc_controller_read_response(client, &received);
    if (result != 0) {
        return result;
    }
    if (response) {
        *response = received;
    }
    if (received.sequence != next) {
        return KMYC_CONTROLLER_ERR_SEQUENCE;
    }
    return received.result == 0 ? KMYC_CONTROLLER_OK : KMYC_CONTROLLER_ERR_DEVICE;
}

int kmyc_controller_apply(kmyc_controller_t *client, const kmyc_controller_state_t *state,
                          kmyc_controller_response_t *response)
{
    if (!state) {
        return KMYC_CONTROLLER_ERR_ARGUMENT;
    }
    uint32_t required = (state->gpio_update_mask ? KMYC_CONTROLLER_CAP_GPIO_LEVEL : 0u) |
                        (state->gpio_mode_update_mask ? KMYC_CONTROLLER_CAP_GPIO_MODE : 0u) |
                        (state->pwm_update ? KMYC_CONTROLLER_CAP_PWM_24KHZ : 0u);
    return command(client, KMYC_CONTROLLER_APPLY_STATE, state, response, required);
}

int kmyc_controller_release_pwm(kmyc_controller_t *client, kmyc_controller_response_t *response)
{
    return command(client, KMYC_CONTROLLER_RELEASE_PWM, NULL, response, KMYC_CONTROLLER_CAP_PWM_24KHZ);
}

int kmyc_controller_release_all(kmyc_controller_t *client, kmyc_controller_response_t *response)
{
    return command(client, KMYC_CONTROLLER_RELEASE_ALL, NULL, response,
                   KMYC_CONTROLLER_CAP_GPIO_MODE | KMYC_CONTROLLER_CAP_PWM_24KHZ);
}

int kmyc_controller_clear_diagnostics(kmyc_controller_t *client, kmyc_controller_response_t *response)
{
    return command(client, KMYC_CONTROLLER_CLEAR_DIAGNOSTICS, NULL, response, 0);
}
