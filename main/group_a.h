#pragma once

#include "esp_err.h"
#include "lighting.h"

#define GROUP_A_LED_COUNT 6

// Internal driver interface. Use lighting.h from application/BLE code.
// Initialization/cleanup run before handoff; updates belong to the lighting task.
esp_err_t group_a_init(void);
esp_err_t group_a_deinit(void);
// Already converted linear RGB duty cycles; no additional brightness scaling.
esp_err_t group_a_set_solid(light_rgb_t pwm);
// Lights one logical position and clears all other LEDs in the same frame.
esp_err_t group_a_set_single(uint8_t position, light_rgb_t pwm);
// Lights position and its next neighbour (5 wraps to 0); clears all others.
esp_err_t group_a_set_pair(uint8_t position, light_rgb_t outgoing, light_rgb_t incoming);
// Writes all six logical positions and refreshes once.
esp_err_t group_a_set_frame(const light_rgb_t pixels[GROUP_A_LED_COUNT]);
