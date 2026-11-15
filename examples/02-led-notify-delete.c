/*
 * 三个 LED + EXTI 按键任务通知 + 依次删除任务
 *
 * 与前面讨论的开源代码逻辑相同，修正：
 *   1. FromISR 唤醒变量初始化
 *   2. 检查空句柄
 *   3. 仅在临界区更新共享目标，不把日志放入临界区
 *   4. 明确 LED 熄灭电平（以下假设 GPIO_PIN_RESET 代表熄灭）
 *
 * 本文件是教学片段，依赖 CubeMX 生成的 main.h/GPIO/EXTI/TIM/FreeRTOS 工程。
 * 如果在其他文件中已有同名 HAL_GPIO_EXTI_Callback()，须合并而不是重复定义。
 * 实际工程仍需处理机械按键消抖和硬件 NVIC 优先级。
 */
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct {
    const char *name;
    uint16_t pin;
    TickType_t periodTicks;
} Led_t;

static TaskHandle_t green_led = NULL;
static TaskHandle_t orange_led = NULL;
static TaskHandle_t red_led = NULL;

/* 同时被任务上下文与 EXTI 中断访问；volatile 不代替临界区同步。 */
static volatile TaskHandle_t next_task_handle = NULL;

/* 示例板卡引脚：STM32F4 Discovery 的 PD12/PD13/PD14。 */
static Led_t green_pin  = {"GREEN",  GPIO_PIN_12, 0};
static Led_t orange_pin = {"ORANGE", GPIO_PIN_13, 0};
static Led_t red_pin    = {"RED",    GPIO_PIN_14, 0};

static void toggle_led_task_handler(void *params)
{
    const Led_t *pin = (const Led_t *)params;

    for (;;) {
        HAL_GPIO_TogglePin(GPIOD, pin->pin);

        /* 无通知：超时后继续闪烁；收到通知：退出循环并删除自己 */
        BaseType_t received = xTaskNotifyWait(
            0, 0, NULL, pin->periodTicks
        );
        if (received != pdTRUE) {
            continue;
        }

        /* 只保护短小的共享句柄更新。ISR 必须以 FreeRTOS 允许的优先级运行。 */
        taskENTER_CRITICAL();
        if (pin->pin == GPIO_PIN_12) {
            next_task_handle = orange_led;
        } else if (pin->pin == GPIO_PIN_13) {
            next_task_handle = red_led;
        } else {
            next_task_handle = NULL;
        }
        taskEXIT_CRITICAL();

        /* 下面假设高电平点亮，低电平熄灭；如为低有效 LED，反转电平。 */
        HAL_GPIO_WritePin(GPIOD, pin->pin, GPIO_PIN_RESET);
        vTaskDelete(NULL);  /* 只删除当前任务，不会返回 */
    }
}

/* 在 GPIO/EXTI 初始化后、启动 FreeRTOS 调度器前调用。 */
void CreateLedNotifyTasks(void)
{
    green_pin.periodTicks  = pdMS_TO_TICKS(1000);
    orange_pin.periodTicks = pdMS_TO_TICKS(800);
    red_pin.periodTicks    = pdMS_TO_TICKS(400);

    configASSERT(green_pin.periodTicks  > 0);
    configASSERT(orange_pin.periodTicks > 0);
    configASSERT(red_pin.periodTicks    > 0);

    BaseType_t status;
    status = xTaskCreate(toggle_led_task_handler, "GREEN", 256,
                         &green_pin, 2, &green_led);
    configASSERT(status == pdPASS);
    status = xTaskCreate(toggle_led_task_handler, "ORANGE", 256,
                         &orange_pin, 2, &orange_led);
    configASSERT(status == pdPASS);
    status = xTaskCreate(toggle_led_task_handler, "RED", 256,
                         &red_pin, 2, &red_led);
    configASSERT(status == pdPASS);

    next_task_handle = green_led;
}

/* STM32 HAL 外部中断回调：需由 HAL_GPIO_EXTI_IRQHandler() 间接调用。 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != GPIO_PIN_0) {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    TaskHandle_t target = next_task_handle;

    if (target != NULL) {
        xTaskNotifyFromISR(target, 0, eNoAction,
                           &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* 警告：
 * 1. GPIO_PIN_0 对应按键 EXTI，需 CubeMX 配置边沿/上拉下拉/NVIC。
 * 2. EXTI 必须满足 configMAX_SYSCALL_INTERRUPT_PRIORITY 的限制。
 * 3. 按键抖动可能一次按下删除多个任务，需使用定时器/软件进行消抖。
 * 4. 高频事件最好不要无限累积；此处仅作“通知状态”教学演示。
 * 5. 被删任务的任务栈/TCB 等内核分配内存由 Idle 任务回收。
 */
