/*
 * 教学片段：队列传递传感器数据，Mutex 保护共享 SPI。
 * 需要适配 STM32 HAL 驱动并由某处调用 CreateIpcObjects()。
 */
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

static QueueHandle_t sensorQueue = NULL;
static SemaphoreHandle_t spiMutex = NULL;

typedef struct {
    uint16_t adc;
    uint32_t timestamp;
} SensorSample;

void CreateIpcObjects(void)
{
    sensorQueue = xQueueCreate(8, sizeof(SensorSample));
    spiMutex = xSemaphoreCreateMutex();
    configASSERT(sensorQueue != NULL && spiMutex != NULL);
}

/* 创建任务时，须先调用 CreateIpcObjects()。 */
static void SensorTask(void *arg)
{
    (void)arg;
    for (;;) {
        SensorSample item = {
            .adc = 1234,                     /* 示例值：替换成实际 ADC 采样 */
            .timestamp = xTaskGetTickCount()
        };
        (void)xQueueSend(sensorQueue, &item, pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void DisplayTask(void *arg)
{
    (void)arg;
    SensorSample item;
    for (;;) {
        if (xQueueReceive(sensorQueue, &item, portMAX_DELAY) == pdPASS) {
            if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                /* SPI_DisplayValue(item.adc, item.timestamp); */
                xSemaphoreGive(spiMutex);
            }
        }
    }
}

/* SensorTask / DisplayTask 为待注册的任务入口示例。
 * 在工程初始化中调用 xTaskCreate() 创建任务即可。
 */
