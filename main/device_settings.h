#pragma once
#include "lighting.h"

typedef enum {
    BOOT_BEHAVIOR_LAST_STATE = 0,
    BOOT_BEHAVIOR_DEFAULT = 1,
    BOOT_BEHAVIOR_OFF = 2,
} boot_behavior_t;

// Initialize before loading lighting settings or starting BLE.
esp_err_t device_settings_init(void);
uint8_t device_settings_get(unsigned field); // 1: boot behavior, 2: indicator enabled
esp_err_t device_settings_set(unsigned field, uint8_t value);
// Apply startup policy after storage load, before submitting any light command.
void device_settings_apply_boot(big_light_settings_t *loaded, const big_light_settings_t *defaults);
