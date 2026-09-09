#pragma once
#include "lighting.h"

// Initializes NVS and save worker. Missing/invalid records leave defaults intact.
esp_err_t light_storage_init(big_light_settings_t *defaults);
// Nonblocking latest-value save; commit after two quiet seconds.
void light_storage_schedule(const big_light_settings_t *settings);

esp_err_t house_storage_init(house_lights_settings_t *defaults);
void house_storage_schedule(const house_lights_settings_t *settings);
