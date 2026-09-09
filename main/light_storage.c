#include "light_storage.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

// Versioned fixed-width record, never the compiler's settings struct layout.
// ESP32 stores these uint32/IEEE float bit patterns little-endian.
typedef struct { uint32_t words[12]; } record_t;
typedef struct {
    QueueHandle_t pending;
    nvs_handle_t handle;
    record_t saved;
    bool have_saved;
    const char *key;
} storage_t;
static storage_t group_a = {.key = "group_a"}, group_b = {.key = "group_b"};
static const char *TAG = "light_storage";
static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float number(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static record_t pack(const big_light_settings_t *s)
{
    return (record_t){{1, s->on, s->effect, bits(s->color.x), bits(s->color.y),
        bits(s->brightness), s->period_ms, s->color_mode, bits(s->gradient_end.x),
        bits(s->gradient_end.y), s->shift_mode, s->shift_period_ms}};
}
static bool unpack(const record_t *r, big_light_settings_t *s)
{
    const uint32_t *w = r->words;
    if (w[0] != 1 || w[1] > 1) return false;
    big_light_settings_t candidate = {.on=w[1], .effect=w[2],
        .color={number(w[3]),number(w[4])}, .brightness=number(w[5]),
        .period_ms=w[6], .color_mode=w[7],
        .gradient_end={number(w[8]),number(w[9])}, .shift_mode=w[10],
        .shift_period_ms=w[11]};
    if (lighting_validate(&candidate) != ESP_OK) return false;
    *s = candidate;
    return true;
}
static void save_task(void *arg)
{
    storage_t *store = arg;
    big_light_settings_t latest;
    for (;;) {
        xQueueReceive(store->pending, &latest, portMAX_DELAY);
        while (xQueueReceive(store->pending, &latest, pdMS_TO_TICKS(2000)) == pdTRUE) {}
        record_t record = pack(&latest);
        if (store->have_saved && memcmp(&record, &store->saved, sizeof(record)) == 0) continue;
        esp_err_t err = nvs_set_blob(store->handle, store->key, &record, sizeof(record));
        if (err == ESP_OK) err = nvs_commit(store->handle);
        if (err == ESP_OK) {
            store->saved = record;
            store->have_saved = true;
            ESP_LOGI(TAG, "%s settings saved", store->key);
        } else {
            ESP_LOGE(TAG, "Settings save failed: %s", esp_err_to_name(err));
        }
    }
}
static esp_err_t storage_init(storage_t *store, big_light_settings_t *defaults)
{
    if (!defaults) return ESP_ERR_INVALID_ARG;
    if (store->pending) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err; // Never erase unrelated NVS automatically.
    err = nvs_open("lighting", NVS_READWRITE, &store->handle);
    if (err != ESP_OK) return err;
    size_t size = sizeof(store->saved);
    err = nvs_get_blob(store->handle, store->key, &store->saved, &size);
    store->have_saved = err == ESP_OK && size == sizeof(store->saved) && (store != &group_b || store->saved.words[2] != LIGHT_EFFECT_LIGHT_HOUSE) && unpack(&store->saved, defaults);
    if (store->have_saved) ESP_LOGI(TAG, "Loaded saved %s settings", store->key);
    else ESP_LOGW(TAG, "No usable saved settings; using main.c defaults (read=%s)", esp_err_to_name(err));
    store->pending = xQueueCreate(1, sizeof(big_light_settings_t));
    if (!store->pending) { nvs_close(store->handle); return ESP_ERR_NO_MEM; }
    if (xTaskCreate(save_task, "light_save", 3072, store, 1, NULL) != pdPASS) {
        vQueueDelete(store->pending); store->pending = NULL; nvs_close(store->handle);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t light_storage_init(big_light_settings_t *defaults) { return storage_init(&group_a, defaults); }
esp_err_t house_storage_init(house_lights_settings_t *defaults) { return storage_init(&group_b, defaults); }
void light_storage_schedule(const big_light_settings_t *s) { if (group_a.pending && s) xQueueOverwrite(group_a.pending, s); }
void house_storage_schedule(const house_lights_settings_t *s) { if (group_b.pending && s) xQueueOverwrite(group_b.pending, s); }
