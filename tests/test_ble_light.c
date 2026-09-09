#include <reent.h>
#include <setjmp.h>
#include <string.h>
#include "../main/lighting.c"
#include "../main/group_b.c"
#include "../main/device_settings.c"
#include "ble_light_protocol.h"

static struct _reent test_reent;
struct _reent *__getreent(void) { return &test_reent; }
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static big_light_settings_t queued;
static bool queue_full;
static unsigned calls;
static jmp_buf task_done;
static bool run_owner;
static unsigned task_step, rendered;
static TickType_t test_now;
static light_rgb_t frame_pixels[4], captures[5][4];
QueueHandle_t xQueueCreate(unsigned length, unsigned size) { (void)length; (void)size; return &queued; }
void vQueueDelete(QueueHandle_t queue) { (void)queue; }
BaseType_t xTaskCreate(TaskFunction_t task, const char *name, unsigned stack,
                      void *arg, unsigned priority, void *handle)
{
    (void)task; (void)name; (void)stack; (void)arg; (void)priority; (void)handle;
    return pdPASS;
}
TickType_t xTaskGetTickCount(void) { return test_now; }
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait)
{
    (void)queue; (void)wait;
    ++calls;
    if (queue_full) return 0;
    queued = *(const big_light_settings_t *)item;
    return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait)
{
    (void)queue; (void)wait;
    if (!run_owner) return 0;
    if (task_step == 5) longjmp(task_done, 1);
    if (task_step++ == 0) {
        *(house_lights_settings_t *)item = (house_lights_settings_t){.on=true,
            .effect=LIGHT_EFFECT_BREATHING, .color=BIG_LIGHT_WHITE, .brightness=1,
            .period_ms=4000};
        return pdTRUE;
    }
    test_now += 100;
    return 0;
}
esp_err_t group_a_init(void) { return ESP_OK; }
esp_err_t group_a_deinit(void) { return ESP_OK; }
esp_err_t group_a_set_solid(light_rgb_t pwm) { (void)pwm; return ESP_OK; }
esp_err_t group_a_set_pair(uint8_t position, light_rgb_t a, light_rgb_t b)
{ (void)position; (void)a; (void)b; return ESP_OK; }
esp_err_t group_a_set_frame(const light_rgb_t pixels[GROUP_A_LED_COUNT])
{ (void)pixels; return ESP_OK; }

esp_err_t led_strip_new_rmt_device(const led_strip_config_t *c, const led_strip_rmt_config_t *r, led_strip_handle_t *h) { (void)r; if(c->max_leds != 4 || c->strip_gpio_num != 6) return ESP_FAIL; *h=(void *)1; return ESP_OK; }
esp_err_t led_strip_clear(led_strip_handle_t h) { (void)h; return ESP_OK; }
esp_err_t led_strip_del(led_strip_handle_t h) { (void)h; return ESP_OK; }
esp_err_t led_strip_set_pixel(led_strip_handle_t h, uint32_t i, uint32_t r, uint32_t g, uint32_t b) { (void)h; if (i >= 4 || r > 255 || g > 255 || b > 255) return ESP_FAIL; frame_pixels[i]=(light_rgb_t){r,g,b}; return ESP_OK; }
esp_err_t led_strip_refresh(led_strip_handle_t h) { (void)h; if (run_owner && rendered < 5) memcpy(captures[rendered++], frame_pixels, sizeof(frame_pixels)); return ESP_OK; }
static uint8_t disk[3], staged[3];
static bool disk_present, fail_commit;
static unsigned commits;
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *h) { (void)name; (void)mode; *h=1; return ESP_OK; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size) {
    (void)h; (void)key;
    if (!disk_present || *size < 3) return ESP_FAIL;
    memcpy(out,disk,3); *size=3; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *in, size_t size) {
    (void)h; (void)key; if(size != 3) return ESP_FAIL; memcpy(staged,in,3); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; ++commits; if(fail_commit) return ESP_FAIL; memcpy(disk,staged,3); disk_present=true; return ESP_OK; }
int main(void)
{
    CHECK(lighting_init() == ESP_OK);
    CHECK(house_lights_init() == ESP_OK);
    ble_light_state_t state = {.big_light = {
        .on = true, .effect = LIGHT_EFFECT_SOLID, .color = BIG_LIGHT_WHITE,
        .brightness = 0.5f, .period_ms = 1500,
        .gradient_end = {0.15f, 0.06f}, .shift_period_ms = 7000,
    }};
    state.house_lights = state.big_light;
    state.house_lights.on = false;
    CHECK(set_big_light(&state.big_light) == ESP_OK);
    uint8_t value[BLE_LIGHT_VALUE_MAX], saved[BLE_LIGHT_VALUE_MAX];
    CHECK(ble_light_read(&state, BLE_LIGHT_INFO, value, sizeof(value)) == 0);
    CHECK(memcmp(value, (uint8_t[]){1, 0x37, 3, 7, 3}, 5) == 0);
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
    // The actual BLE path must accept Breathing, not only the local setter.
    CHECK(ble_light_write(&state, BLE_LIGHT_EFFECT, (uint8_t[]){5}, 1) == BLE_LIGHT_OK);
    CHECK(state.big_light.effect == LIGHT_EFFECT_BREATHING && queued.effect == LIGHT_EFFECT_BREATHING);
    CHECK(ble_light_read(&state, BLE_LIGHT_EFFECT, value, sizeof(value)) == BLE_LIGHT_OK && value[0] == 5);
    CHECK(ble_light_write(&state, BLE_LIGHT_PERIOD, (uint8_t[4]){0}, 4) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_write(&state, BLE_LIGHT_EFFECT, (uint8_t[]){1}, 1) == BLE_LIGHT_OK);
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
    CHECK(ble_light_write(&state, BLE_LIGHT_HOUSE_ON, (uint8_t[]){1}, 1) == BLE_LIGHT_OK);
    CHECK(state.house_lights.on);
    CHECK(ble_light_write(&state, BLE_HOUSE_EFFECT, (uint8_t[]){1}, 1) == BLE_LIGHT_NOT_SUPPORTED);
    CHECK(ble_light_write(&state, BLE_HOUSE_EFFECT, (uint8_t[]){5}, 1) == BLE_LIGHT_OK);
    CHECK(state.big_light.effect == LIGHT_EFFECT_LIGHT_HOUSE);
    CHECK(ble_light_write(&state, BLE_LIGHT_HOUSE_ON, (uint8_t[]){0}, 1) == BLE_LIGHT_OK);
    CHECK(ble_light_write(&state, BLE_LIGHT_HOUSE_ON, (uint8_t[]){2}, 1) == BLE_LIGHT_VALUE_NOT_ALLOWED);
    CHECK(ble_light_read(&state, BLE_LIGHT_HOUSE_ON, value, sizeof(value)) == 0 && value[0] == 0);
    CHECK(ble_light_read(&state, BLE_HOUSE_INFO, value, sizeof(value)) == BLE_LIGHT_OK);
    CHECK(memcmp(value, (uint8_t[]){1, 0x35, 3, 7, 3}, 5) == 0);
    for (int field = BLE_LIGHT_HOUSE_ON; field <= BLE_HOUSE_SHIFT_PERIOD; ++field) {
        size_t size = ble_light_field_size(field);
        CHECK(ble_light_read(&state, field, value, sizeof(value)) == BLE_LIGHT_OK);
        CHECK(ble_light_write(&state, field, value, size) == BLE_LIGHT_OK);
        CHECK(ble_light_write(&state, field, value, size - 1) == BLE_LIGHT_INVALID_LENGTH);
    }
    // Four-position gradient uses the whole A->B->A loop, with a guard after the output.
    struct { light_rgb_t pixels[4]; uint32_t guard; } four = {.guard = 0x12345678};
    house_lights_settings_t h = state.house_lights;
    h.on = true; h.color_mode = LIGHT_COLOR_GRADIENT; h.shift_mode = LIGHT_SHIFT_STATIC;
    CHECK(color_pattern_render_count(&h, 0, 1, four.pixels, 4) == ESP_OK);
    light_rgb_t endpoint;
    CHECK(light_color_to_pwm(h.gradient_end, h.brightness, &endpoint) == ESP_OK);
    CHECK(memcmp(&endpoint, &four.pixels[2], sizeof(endpoint)) == 0);
    candle_render_count(1000, 42, (light_rgb_t){255,255,255}, four.pixels, 4);
    CHECK(four.guard == 0x12345678);
    sparkles_render_count(1000, 42, (light_rgb_t){255,255,255}, four.pixels, 4);
    CHECK(four.guard == 0x12345678);
    run_owner = true;
    if (setjmp(task_done) == 0) owner(NULL);
    run_owner = false;
    CHECK(rendered == 5);
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(captures[0][i].r == 0 && captures[4][i].r == 0);
        CHECK(captures[2][i].r >= 254);
        CHECK(captures[1][i].r == 63);
    }
    big_light_queue = NULL;
    CHECK(ble_light_write(&state, BLE_LIGHT_ON, (uint8_t[]){1}, 1) == BLE_LIGHT_UNLIKELY);
    CHECK(!state.big_light.on);
    CHECK(ble_light_field_size((ble_light_field_t)99) == 0);
    CHECK(ble_light_read(NULL, BLE_LIGHT_ON, value, sizeof(value)) == BLE_LIGHT_UNLIKELY);
    CHECK(ble_light_write(NULL, BLE_LIGHT_ON, value, 1) == BLE_LIGHT_UNLIKELY);
    CHECK(device_settings_init() == ESP_OK);
    CHECK(device_settings_get(1) == BOOT_BEHAVIOR_LAST_STATE && device_settings_get(2) == 1);
    big_light_settings_t defaults = {.on=true, .effect=LIGHT_EFFECT_SOLID};
    big_light_settings_t restored = {.on=true, .effect=LIGHT_EFFECT_BREATHING};
    device_settings_apply_boot(&restored, &defaults);
    CHECK(restored.effect == LIGHT_EFFECT_BREATHING && restored.on);
    CHECK(device_settings_set(1, BOOT_BEHAVIOR_OFF) == ESP_OK);
    device_settings_apply_boot(&restored, &defaults);
    CHECK(!restored.on && restored.effect == LIGHT_EFFECT_BREATHING);
    CHECK(device_settings_set(1, BOOT_BEHAVIOR_DEFAULT) == ESP_OK);
    device_settings_apply_boot(&restored, &defaults);
    CHECK(restored.on && restored.effect == LIGHT_EFFECT_SOLID);
    CHECK(device_settings_set(2, 0) == ESP_OK);
    unsigned before_commits = commits;
    CHECK(device_settings_set(2, 0) == ESP_OK && commits == before_commits);
    CHECK(device_settings_set(1, 3) == ESP_ERR_INVALID_ARG);
    CHECK(device_settings_set(2, 2) == ESP_ERR_INVALID_ARG);
    CHECK(device_settings_set(0, 0) == ESP_ERR_INVALID_ARG);
    fail_commit=true;
    CHECK(device_settings_set(2, 1) == ESP_FAIL && device_settings_get(2) == 0);
    fail_commit=false;
    ready=false;
    CHECK(device_settings_init() == ESP_OK);
    CHECK(device_settings_get(1) == BOOT_BEHAVIOR_DEFAULT && device_settings_get(2) == 0);
    return 0;
}
