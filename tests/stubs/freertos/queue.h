#pragma once
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned length, unsigned size);
void vQueueDelete(QueueHandle_t queue);
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait);
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t wait);
