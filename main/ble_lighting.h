#pragma once

#include "lighting.h"

// Call once after lighting_init() and a successful startup set_big_light().
// Seeds GATT readback with that accepted command. After startup the NimBLE host
// owns BLE command state; don't bypass it with later local setter calls.
// ESP_OK means the host task started; advertising begins on host sync.
esp_err_t ble_lighting_init(const big_light_settings_t *startup);
