#include "device_settings.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

// One versioned record commits both settings together; no C struct layout on flash.
static uint8_t state[3] = {1, BOOT_BEHAVIOR_LAST_STATE, 1};
static nvs_handle_t handle;
static bool ready;

esp_err_t device_settings_init(void)
{
    if (ready) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err;
    err = nvs_open("device_settings", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    uint8_t saved[3];
    size_t size = sizeof(saved);
    err = nvs_get_blob(handle, "config", saved, &size);
    if (err == ESP_OK && size == sizeof(saved) && saved[0] == 1 &&
        saved[1] <= BOOT_BEHAVIOR_OFF && saved[2] <= 1) {
        memcpy(state, saved, sizeof(state));
    } else {
        ESP_LOGW("device_settings", "No valid device settings; using LAST_STATE and BLE indicators enabled");
    }
    ready = true;
    return ESP_OK;
}

uint8_t device_settings_get(unsigned field)
{
    return field == 1 || field == 2 ? state[field] : 0;
}

esp_err_t device_settings_set(unsigned field, uint8_t value)
{
    if ((field != 1 && field != 2) || value > (field == 1 ? 2 : 1)) return ESP_ERR_INVALID_ARG;
    if (!ready) return ESP_ERR_INVALID_STATE;
    if (state[field] == value) return ESP_OK;
    uint8_t candidate[3];
    memcpy(candidate, state, sizeof(candidate));
    candidate[field] = value;
    esp_err_t err = nvs_set_blob(handle, "config", candidate, sizeof(candidate));
    if (err == ESP_OK) err = nvs_commit(handle);
    if (err == ESP_OK) memcpy(state, candidate, sizeof(state));
    else ESP_LOGE("device_settings", "Save failed: %s", esp_err_to_name(err));
    return err;
}

void device_settings_apply_boot(big_light_settings_t *loaded, const big_light_settings_t *defaults)
{
    if (state[1] == BOOT_BEHAVIOR_DEFAULT) *loaded = *defaults;
    else if (state[1] == BOOT_BEHAVIOR_OFF) loaded->on = false;
}
