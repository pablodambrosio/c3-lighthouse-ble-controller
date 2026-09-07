#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define configTICK_RATE_HZ 100
#define pdMS_TO_TICKS(ms) ((TickType_t)((ms) / 10))
