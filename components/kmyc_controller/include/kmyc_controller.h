#ifndef KMYC_CONTROLLER_H
#define KMYC_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KMYC_CONTROLLER_I2C_ADDRESS 0x2cu
#define KMYC_CONTROLLER_CAP_GPIO_LEVEL (1u << 0)
#define KMYC_CONTROLLER_CAP_GPIO_MODE (1u << 1)
#define KMYC_CONTROLLER_CAP_PWM_24KHZ (1u << 2)
#define KMYC_CONTROLLER_CAP_DESCRIPTOR_TLV (1u << 3)
#define KMYC_CONTROLLER_CAP_DESCRIPTOR_AB (1u << 4)
#define KMYC_CONTROLLER_CAP_WATCHDOG (1u << 5)
#define KMYC_CONTROLLER_STATUS_READY (1u << 0)
#define KMYC_CONTROLLER_STATUS_DESCRIPTOR_VALID (1u << 1)
#define KMYC_CONTROLLER_STATUS_CONFIG_UNLOCKED (1u << 2)
#define KMYC_CONTROLLER_STATUS_CONFIG_BUSY (1u << 3)
#define KMYC_CONTROLLER_STATUS_I2C_ERROR (1u << 4)
#define KMYC_CONTROLLER_DESCRIPTOR_SIZE 128u
#define KMYC_CONTROLLER_PAYLOAD_MAX 108u
#define KMYC_CONTROLLER_GPIO_LCD_RST 0u
#define KMYC_CONTROLLER_GPIO_TP_RST 1u
#define KMYC_CONTROLLER_GPIO_TP_INT 2u
#define KMYC_CONTROLLER_MODE_INPUT 0u
#define KMYC_CONTROLLER_MODE_PUSH_PULL 1u
#define KMYC_CONTROLLER_MODE_OPEN_DRAIN 2u
#define KMYC_CONTROLLER_TLV_DISPLAY_PRODUCT_ID 1u
#define KMYC_CONTROLLER_TLV_DISPLAY_CONTROLLER 2u
#define KMYC_CONTROLLER_TLV_TOUCH_PRODUCT_ID 3u
#define KMYC_CONTROLLER_TLV_TOUCH_CONTROLLER 4u
#define KMYC_CONTROLLER_TLV_DIMENSIONS 5u
#define KMYC_CONTROLLER_TLV_INTERFACE 6u
#define KMYC_CONTROLLER_TLV_DEFAULT_ORIENTATION 7u
#define KMYC_CONTROLLER_TLV_TOUCH_I2C_ADDRESSES 8u

typedef enum {
    KMYC_CONTROLLER_OK = 0,
    KMYC_CONTROLLER_TLV_END = 1,
    KMYC_CONTROLLER_ERR_ARGUMENT = -1,
    KMYC_CONTROLLER_ERR_TRANSPORT = -2,
    KMYC_CONTROLLER_ERR_PROTOCOL = -3,
    KMYC_CONTROLLER_ERR_CRC = -4,
    KMYC_CONTROLLER_ERR_SEQUENCE = -5,
    KMYC_CONTROLLER_ERR_DEVICE = -6,
    KMYC_CONTROLLER_ERR_UNSUPPORTED = -7,
    KMYC_CONTROLLER_ERR_TLV = -8,
    KMYC_CONTROLLER_ERR_NOT_PROBED = -9
} kmyc_controller_error_t;

typedef enum {
    KMYC_CONTROLLER_APPLY_STATE = 1,
    KMYC_CONTROLLER_RELEASE_PWM = 2,
    KMYC_CONTROLLER_RELEASE_ALL = 3,
    KMYC_CONTROLLER_CLEAR_DIAGNOSTICS = 4
} kmyc_controller_opcode_t;

typedef struct {
    void *context;
    /* Both callbacks return 0 on success. Context owns the device at 7-bit 0x2C.
     * read: register-pointer write + repeated-start read. write: register byte
     * followed by ALL payload bytes in one transaction ending with STOP.
     * A control write must finish STOP before its separate response read. */
    int (*read)(void *context, uint8_t reg, uint8_t *data, size_t length);
    int (*write)(void *context, uint8_t reg, const uint8_t *data, size_t length);
} kmyc_controller_transport_t;

typedef struct {
    uint8_t protocol_major, protocol_minor;
    uint8_t firmware_major, firmware_minor, firmware_patch;
    uint8_t hardware_major, hardware_minor, device_type;
    uint32_t capabilities, uptime_ms;
    uint8_t reset_cause, status, last_error, last_sequence;
    uint8_t gpio_latch, gpio_modes, gpio_inputs_raw, pwm_duty;
    uint16_t pwm_frequency_hz;
    uint8_t pwm_state, descriptor_source;
    uint32_t descriptor_generation;
    uint16_t descriptor_length, i2c_error_count;
} kmyc_controller_info_t;

typedef struct {
    uint8_t sequence, result;
    uint8_t gpio_latch, gpio_modes, gpio_inputs_raw;
    uint8_t pwm_state, pwm_duty, status, last_error;
} kmyc_controller_response_t;

typedef struct {
    uint8_t gpio_update_mask, gpio_values, gpio_mode_update_mask, gpio_modes;
    bool pwm_update;
    uint8_t pwm_duty;
} kmyc_controller_state_t;

typedef struct {
    uint8_t format_version, flags;
    uint16_t payload_length;
    uint32_t generation, payload_crc32;
    uint8_t payload[KMYC_CONTROLLER_PAYLOAD_MAX];
} kmyc_controller_descriptor_t;

typedef struct {
    uint8_t type, length;
    const uint8_t *value; /* Binary slice, not necessarily NUL-terminated text. */
} kmyc_controller_tlv_t;

typedef struct {
    kmyc_controller_transport_t transport;
    uint32_t capabilities;
    uint8_t sequence;
    bool probed;
    int last_transport_error;
} kmyc_controller_t;

/* The caller serializes ALL operations on one instance, including descriptor
 * reads and command write/response read pairs. No allocation or hidden retries. */
int kmyc_controller_init(kmyc_controller_t *client, const kmyc_controller_transport_t *transport);
int kmyc_controller_probe(kmyc_controller_t *client, kmyc_controller_info_t *info);
bool kmyc_controller_is_supported(const kmyc_controller_info_t *info, uint32_t required_capabilities);
int kmyc_controller_read_info(kmyc_controller_t *client, kmyc_controller_info_t *info);
int kmyc_controller_read_response(kmyc_controller_t *client, kmyc_controller_response_t *response);
int kmyc_controller_read_descriptor(kmyc_controller_t *client, kmyc_controller_descriptor_t *descriptor);
/* Commands always read/validate the response; their response output may be NULL. */
int kmyc_controller_apply(kmyc_controller_t *client, const kmyc_controller_state_t *state,
                          kmyc_controller_response_t *response);
int kmyc_controller_release_pwm(kmyc_controller_t *client, kmyc_controller_response_t *response);
int kmyc_controller_release_all(kmyc_controller_t *client, kmyc_controller_response_t *response);
int kmyc_controller_clear_diagnostics(kmyc_controller_t *client, kmyc_controller_response_t *response);

/* Pure helpers, independent of ESP-IDF. Decode does not modify output on error. */
uint8_t kmyc_controller_crc8(const uint8_t *data, size_t length);
uint32_t kmyc_controller_crc32(const uint8_t *data, size_t length);
int kmyc_controller_decode_info(const uint8_t data[48], kmyc_controller_info_t *info);
int kmyc_controller_encode_control(uint8_t sequence, kmyc_controller_opcode_t opcode,
                                   const kmyc_controller_state_t *state, uint8_t data[16]);
int kmyc_controller_decode_response(const uint8_t data[16], kmyc_controller_response_t *response);
int kmyc_controller_decode_descriptor(const uint8_t data[128], kmyc_controller_descriptor_t *descriptor);
int kmyc_controller_tlv_next(const kmyc_controller_descriptor_t *descriptor, size_t *cursor,
                             kmyc_controller_tlv_t *tlv);

/* No descriptor/configuration write API is intentionally provided. */
#ifdef __cplusplus
}
#endif
#endif
