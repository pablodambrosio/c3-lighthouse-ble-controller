#pragma once
#include "FreeRTOS.h"
typedef void (*TaskFunction_t)(void *);
BaseType_t xTaskCreate(TaskFunction_t task, const char *name, unsigned stack,
                      void *argument, unsigned priority, void *handle);
TickType_t xTaskGetTickCount(void);
