/*
 * 教学片段：同一个入口函数创建三个独立 FreeRTOS 任务。
 * 前提：已经在 STM32CubeMX 工程中启用 FreeRTOS，配置 GPIOD LED 引脚。
 * 不能直接把本文件当作可编译的独立 main.c。
 */
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct {
    const char *name;
    uint16_t pin;
    TickType_t periodTicks;
} LedConfig;

static TaskHandle_t greenHandle = NULL;
static TaskHandle_t orangeHandle = NULL;
static TaskHandle_t redHandle = NULL;

/* 以下 GPIO 引脚按常见 STM32F4 Discovery LED 连接示例；修改为自己的板卡。 */
static LedConfig greenCfg  = {"Green",  GPIO_PIN_12, 0};
static LedConfig orangeCfg = {"Orange", GPIO_PIN_13, 0};
static LedConfig redCfg    = {"Red",    GPIO_PIN_14, 0};

static void led_task(void *argument)
{
    const LedConfig *cfg = (const LedConfig *)argument;
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        HAL_GPIO_TogglePin(GPIOD, cfg->pin);
        vTaskDelayUntil(&lastWake, cfg->periodTicks);
    }
}

/* 在 GPIO 初始化后、启动调度器前调用。 */
void CreateLedTasks(void)
{
    greenCfg.periodTicks  = pdMS_TO_TICKS(1000);
    orangeCfg.periodTicks = pdMS_TO_TICKS(800);
    redCfg.periodTicks    = pdMS_TO_TICKS(400);

    configASSERT(greenCfg.periodTicks  > 0);
    configASSERT(orangeCfg.periodTicks > 0);
    configASSERT(redCfg.periodTicks    > 0);

    BaseType_t status;
    status = xTaskCreate(led_task, "Green", 256,
                         &greenCfg, 2, &greenHandle);
    configASSERT(status == pdPASS);
    status = xTaskCreate(led_task, "Orange", 256,
                         &orangeCfg, 2, &orangeHandle);
    configASSERT(status == pdPASS);
    status = xTaskCreate(led_task, "Red", 256,
                         &redCfg, 2, &redHandle);
    configASSERT(status == pdPASS);
}

/* 示例：由 main() 在完成外设初始化后调用 CreateLedTasks()，
 * 然后调用 vTaskStartScheduler()。CMSIS-RTOS 工程应遵循对应启动流程。 */
