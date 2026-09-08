#include <reent.h>
#include <string.h>
#include "../main/lighting.c"
#include "ble_light_protocol.h"

static struct _reent test_reent;
struct _reent *__getreent(void) { return &test_reent; }
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static big_light_settings_t queued;
static bool queue_full;
static unsigned calls;
QueueHandle_t xQueueCreate(unsigned length, unsigned size) { (void)length; (void)size; return &queued; }
void vQueueDelete(QueueHandle_t queue) { (void)queue; }
BaseType_t xTaskCreate(TaskFunction_t task, const char *name, unsigned stack,
                      void *arg, unsigned priority, void *handle)
{
    (void)task; (void)name; (void)stack; (void)arg; (void)priority; (void)handle;
    return pdPASS;
}
TickType_t xTaskGetTickCount(void) { return 0; }
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait)
{
    (void)queue; (void)wait;
    ++calls;
    if (queue_full) return 0;
    queued = *(const big_light_settings_t *)item;
    return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait)
{ (void)queue; (void)item; (void)wait; return 0; }
esp_err_t group_a_init(void) { return ESP_OK; }
esp_err_t group_a_deinit(void) { return ESP_OK; }
esp_err_t group_a_set_solid(light_rgb_t pwm) { (void)pwm; return ESP_OK; }
esp_err_t group_a_set_pair(uint8_t position, light_rgb_t a, light_rgb_t b)
{ (void)position; (void)a; (void)b; return ESP_OK; }
esp_err_t group_a_set_frame(const light_rgb_t pixels[GROUP_A_LED_COUNT])
{ (void)pixels; return ESP_OK; }

int main(void)
{
    CHECK(lighting_init() == ESP_OK);
    ble_light_state_t state = {.big_light = {
        .on = true, .effect = LIGHT_EFFECT_SOLID, .color = BIG_LIGHT_WHITE,
        .brightness = 0.5f, .period_ms = 1500,
        .gradient_end = {0.15f, 0.06f}, .shift_period_ms = 7000,
    }};
    CHECK(set_big_light(&state.big_light) == ESP_OK);
    uint8_t value[BLE_LIGHT_VALUE_MAX], saved[BLE_LIGHT_VALUE_MAX];
    CHECK(ble_light_read(&state, BLE_LIGHT_INFO, value, sizeof(value)) == 0);
    CHECK(memcmp(value, (uint8_t[]){1, 0x17, 3, 7, 1}, 5) == 0);
    CHECK(ble_light_read(&state, BLE_LIGHT_BRIGHTNESS, value, sizeof(value)) == 0);
    CHECK(memcmp(value, (uint8_t[]){0, 0, 0, 0x3f}, 4) == 0);
    CHECK(ble_light_read(&state, BLE_LIGHT_PERIOD, value, sizeof(value)) == 0);
    CHECK(memcmp(value, (uint8_t[]){0xdc, 5, 0, 0}, 4) == 0);
    for (int field = BLE_LIGHT_ON; field <= BLE_LIGHT_SHIFT_PERIOD; ++field) {
        size_t size = ble_light_field_size(field);
        CHECK(ble_light_read(&state, field, value, sizeof(value)) == 0);
        memcpy(saved, value, size);
        unsigned before = calls;
        CHECK(ble_light_write(&state, field, value, size) == 0);
        CHECK(calls == before + 1);
        CHECK(ble_light_read(&state, field, value, sizeof(value)) == 0);
        CHECK(memcmp(saved, value, size) == 0);
        CHECK(ble_light_write(&state, field, value, size - 1) == BLE_LIGHT_INVALID_LENGTH);
        CHECK(ble_light_write(&state, field, value, size + 1) == BLE_LIGHT_INVALID_LENGTH);
        CHECK(ble_light_write(&state, field, NULL, size) == BLE_LIGHT_INVALID_LENGTH);
        CHECK(ble_light_read(&state, field, value, size - 1) == BLE_LIGHT_INVALID_LENGTH);
    }
    CHECK(ble_light_write(&state, BLE_LIGHT_INFO, value, 5) == BLE_LIGHT_WRITE_NOT_PERMITTED);
    CHECK(ble_light_write(&state, BLE_LIGHT_ON, (uint8_t[]){2}, 1) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_write(&state, BLE_LIGHT_EFFECT, (uint8_t[]){3}, 1) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_write(&state, BLE_LIGHT_COLOR_MODE, (uint8_t[]){2}, 1) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_write(&state, BLE_LIGHT_SHIFT_MODE, (uint8_t[]){3}, 1) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    // Invalid float encodings: NaN, infinity, negative and >1 brightness.
    const uint8_t bad_brightness[][4] = {{0, 0, 0xc0, 0x7f}, {0, 0, 0x80, 0x7f},
                                        {0, 0, 0, 0xbf}, {0, 0, 0xc0, 0x3f}};
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(ble_light_write(&state, BLE_LIGHT_BRIGHTNESS, bad_brightness[i], 4) == BLE_LIGHT_VALUE_NOT_ALLOWED);
        CHECK(state.big_light.brightness == 0.5f);
    }
    CHECK(ble_light_write(&state, BLE_LIGHT_COLOR, (uint8_t[8]){0}, 8) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_write(&state, BLE_LIGHT_GRADIENT_END, (uint8_t[8]){0}, 8) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_write(&state, BLE_LIGHT_EFFECT, (uint8_t[]){1}, 1) == 0);
    CHECK(ble_light_write(&state, BLE_LIGHT_PERIOD, (uint8_t[4]){0}, 4) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(state.big_light.period_ms == 1500);
    CHECK(ble_light_write(&state, BLE_LIGHT_COLOR_MODE, (uint8_t[]){1}, 1) == 0);
    CHECK(ble_light_write(&state, BLE_LIGHT_SHIFT_MODE, (uint8_t[]){1}, 1) == 0);
    CHECK(ble_light_write(&state, BLE_LIGHT_SHIFT_PERIOD, (uint8_t[4]){0}, 4) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_write(&state, BLE_LIGHT_PERIOD, (uint8_t[]){0xff, 0xff, 0xff, 0xff}, 4) == 0);
    CHECK(state.big_light.period_ms == UINT32_MAX && queued.period_ms == UINT32_MAX);
    queue_full = true;
    CHECK(ble_light_write(&state, BLE_LIGHT_ON, (uint8_t[]){0}, 1) == BLE_LIGHT_INSUFFICIENT_RESOURCES);
    CHECK(state.big_light.on && queued.on);
    queue_full = false;
    CHECK(ble_light_write(&state, BLE_LIGHT_ON, (uint8_t[]){0}, 1) == 0);
    CHECK(!state.big_light.on && !queued.on);
    CHECK(ble_light_write(&state, BLE_LIGHT_HOUSE_ON, (uint8_t[]){1}, 1) == BLE_LIGHT_NOT_SUPPORTED);
    CHECK(!state.house_lights.on);
    CHECK(ble_light_write(&state, BLE_LIGHT_HOUSE_ON, (uint8_t[]){2}, 1) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_read(&state, BLE_LIGHT_HOUSE_ON, value, sizeof(value)) == 0 && value[0] == 0);
    big_light_queue = NULL;
    CHECK(ble_light_write(&state, BLE_LIGHT_ON, (uint8_t[]){1}, 1) == BLE_LIGHT_UNLIKELY);
    CHECK(!state.big_light.on);
    CHECK(ble_light_field_size((ble_light_field_t)99) == 0);
    CHECK(ble_light_read(NULL, BLE_LIGHT_ON, value, sizeof(value)) == BLE_LIGHT_UNLIKELY);
    CHECK(ble_light_write(NULL, BLE_LIGHT_ON, value, 1) == BLE_LIGHT_UNLIKELY);
    return 0;
}
