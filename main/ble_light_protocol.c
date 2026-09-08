#include "ble_light_protocol.h"

#include <float.h>
#include <string.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
               FLT_MAX_EXP == 128, "BLE protocol requires IEEE-754 binary32");

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}

static float read_float(const uint8_t *p)
{
    uint32_t bits = read_u32(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void write_float(uint8_t *p, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32(p, bits);
}

size_t ble_light_field_size(ble_light_field_t field)
{
    switch (field) {
    case BLE_LIGHT_INFO: return 5;
    case BLE_LIGHT_COLOR:
    case BLE_LIGHT_GRADIENT_END: return 8;
    case BLE_LIGHT_BRIGHTNESS:
    case BLE_LIGHT_PERIOD:
    case BLE_LIGHT_SHIFT_PERIOD: return 4;
    case BLE_LIGHT_ON:
    case BLE_LIGHT_EFFECT:
    case BLE_LIGHT_COLOR_MODE:
    case BLE_LIGHT_SHIFT_MODE:
    case BLE_LIGHT_HOUSE_ON: return 1;
    default: return 0;
    }
}

const char *ble_light_field_name(ble_light_field_t field)
{
    switch (field) {
    case BLE_LIGHT_INFO: return "Protocol and capabilities";
    case BLE_LIGHT_ON: return "Big light on";
    case BLE_LIGHT_EFFECT: return "Effect";
    case BLE_LIGHT_COLOR: return "Color xy";
    case BLE_LIGHT_BRIGHTNESS: return "Brightness";
    case BLE_LIGHT_PERIOD: return "Rotation period ms";
    case BLE_LIGHT_COLOR_MODE: return "Color mode";
    case BLE_LIGHT_GRADIENT_END: return "Gradient end xy";
    case BLE_LIGHT_SHIFT_MODE: return "Shift mode";
    case BLE_LIGHT_SHIFT_PERIOD: return "Shift period ms";
    case BLE_LIGHT_HOUSE_ON: return "House lights on (unsupported)";
    default: return "Unknown";
    }
}

int ble_light_read(const ble_light_state_t *state, ble_light_field_t field,
                   uint8_t *value, size_t capacity)
{
    size_t size = ble_light_field_size(field);
    if (!state || !value || !size) return BLE_LIGHT_UNLIKELY;
    if (capacity < size) return BLE_LIGHT_INVALID_LENGTH;
    const big_light_settings_t *s = &state->big_light;
    switch (field) {
    case BLE_LIGHT_INFO:
        // Version, effect bitmask, color-mode bitmask, shift-mode bitmask,
        // supported groups bitmask (bit 0 = A; bit 1 = B).
        memcpy(value, (uint8_t[]){1, 0x17, 0x03, 0x07, 0x01}, 5);
        break;
    case BLE_LIGHT_ON: value[0] = s->on; break;
    case BLE_LIGHT_EFFECT: value[0] = s->effect; break;
    case BLE_LIGHT_COLOR: write_float(value, s->color.x); write_float(value + 4, s->color.y); break;
    case BLE_LIGHT_BRIGHTNESS: write_float(value, s->brightness); break;
    case BLE_LIGHT_PERIOD: write_u32(value, s->period_ms); break;
    case BLE_LIGHT_COLOR_MODE: value[0] = s->color_mode; break;
    case BLE_LIGHT_GRADIENT_END: write_float(value, s->gradient_end.x); write_float(value + 4, s->gradient_end.y); break;
    case BLE_LIGHT_SHIFT_MODE: value[0] = s->shift_mode; break;
    case BLE_LIGHT_SHIFT_PERIOD: write_u32(value, s->shift_period_ms); break;
    case BLE_LIGHT_HOUSE_ON: value[0] = state->house_lights.on; break;
    default: return BLE_LIGHT_UNLIKELY;
    }
    return BLE_LIGHT_OK;
}

static int att_error(esp_err_t err)
{
    switch (err) {
    case ESP_OK: return BLE_LIGHT_OK;
    case ESP_ERR_INVALID_ARG: return BLE_LIGHT_VALUE_NOT_ALLOWED;
    case ESP_ERR_NOT_SUPPORTED: return BLE_LIGHT_NOT_SUPPORTED;
    case ESP_ERR_TIMEOUT:
    case ESP_ERR_NO_MEM: return BLE_LIGHT_INSUFFICIENT_RESOURCES;
    default: return BLE_LIGHT_UNLIKELY;
    }
}

int ble_light_write(ble_light_state_t *state, ble_light_field_t field,
                    const uint8_t *value, size_t length)
{
    if (!state) return BLE_LIGHT_UNLIKELY;
    if (field == BLE_LIGHT_INFO) return BLE_LIGHT_WRITE_NOT_PERMITTED;
    size_t size = ble_light_field_size(field);
    if (!size) return BLE_LIGHT_UNLIKELY;
    if (!value || length != size) return BLE_LIGHT_INVALID_LENGTH;
    big_light_settings_t candidate = state->big_light;
    switch (field) {
    case BLE_LIGHT_ON:
    case BLE_LIGHT_HOUSE_ON:
        if (value[0] > 1) return BLE_LIGHT_VALUE_NOT_ALLOWED;
        if (field == BLE_LIGHT_HOUSE_ON) {
            house_lights_settings_t house = {.on = value[0] != 0};
            int result = att_error(set_house_lights(&house));
            if (!result) state->house_lights = house;
            return result;
        }
        candidate.on = value[0] != 0;
        break;
    case BLE_LIGHT_EFFECT: candidate.effect = (light_effect_t)value[0]; break;
    case BLE_LIGHT_COLOR:
    case BLE_LIGHT_GRADIENT_END: {
        light_xy_t xy = {read_float(value), read_float(value + 4)};
        light_rgb_t ignored;
        // Reject malformed coordinates even when this endpoint is inactive.
        if (light_color_to_pwm(xy, 1.0f, &ignored) != ESP_OK) return BLE_LIGHT_VALUE_NOT_ALLOWED;
        if (field == BLE_LIGHT_COLOR) candidate.color = xy;
        else candidate.gradient_end = xy;
        break;
    }
    case BLE_LIGHT_BRIGHTNESS: candidate.brightness = read_float(value); break;
    case BLE_LIGHT_PERIOD: candidate.period_ms = read_u32(value); break;
    case BLE_LIGHT_COLOR_MODE: candidate.color_mode = (light_color_mode_t)value[0]; break;
    case BLE_LIGHT_SHIFT_MODE: candidate.shift_mode = (light_shift_mode_t)value[0]; break;
    case BLE_LIGHT_SHIFT_PERIOD: candidate.shift_period_ms = read_u32(value); break;
    default: return BLE_LIGHT_UNLIKELY;
    }
    int result = att_error(set_big_light(&candidate));
    if (!result) state->big_light = candidate;
    return result;
}
