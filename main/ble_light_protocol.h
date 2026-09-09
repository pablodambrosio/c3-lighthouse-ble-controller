#pragma once

#include <stddef.h>
#include "lighting.h"

// Characteristic UUID number in 8e7fNNNN-8f58-4b5c-9d76-2f5a37c41000.
typedef enum {
    BLE_LIGHT_INFO = 1,
    BLE_LIGHT_ON,
    BLE_LIGHT_EFFECT,
    BLE_LIGHT_COLOR,
    BLE_LIGHT_BRIGHTNESS,
    BLE_LIGHT_PERIOD,
    BLE_LIGHT_COLOR_MODE,
    BLE_LIGHT_GRADIENT_END,
    BLE_LIGHT_SHIFT_MODE,
    BLE_LIGHT_SHIFT_PERIOD,
    BLE_LIGHT_HOUSE_ON,
    BLE_HOUSE_EFFECT, BLE_HOUSE_COLOR, BLE_HOUSE_BRIGHTNESS, BLE_HOUSE_PERIOD,
    BLE_HOUSE_COLOR_MODE, BLE_HOUSE_GRADIENT_END, BLE_HOUSE_SHIFT_MODE, BLE_HOUSE_SHIFT_PERIOD,
    BLE_HOUSE_INFO,
} ble_light_field_t;

#define BLE_LIGHT_VALUE_MAX 8

// ATT error values, also used by the stack-independent tests.
enum {
    BLE_LIGHT_OK = 0,
    BLE_LIGHT_WRITE_NOT_PERMITTED = 0x03,
    BLE_LIGHT_INVALID_LENGTH = 0x0d,
    BLE_LIGHT_UNLIKELY = 0x0e,
    BLE_LIGHT_INSUFFICIENT_RESOURCES = 0x11,
    BLE_LIGHT_VALUE_NOT_ALLOWED = 0x13,
    BLE_LIGHT_NOT_SUPPORTED = 0x80,
};

typedef struct {
    big_light_settings_t big_light;
    house_lights_settings_t house_lights;
} ble_light_state_t;

size_t ble_light_field_size(ble_light_field_t field);
const char *ble_light_field_name(ble_light_field_t field);
int ble_light_read(const ble_light_state_t *state, ble_light_field_t field,
                   uint8_t *value, size_t capacity);
// Serialize callers. Cache is updated only after the lighting setter accepts.
int ble_light_write(ble_light_state_t *state, ble_light_field_t field,
                    const uint8_t *value, size_t length);
