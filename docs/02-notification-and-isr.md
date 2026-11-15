# 02 · 任务通知、ISR 与按键删除任务

## 两种任务通知接收方式

| `xTaskNotifyWait()` | `ulTaskNotifyTake()` |
|---|---|
| 检查通知**状态** Pending | 检查通知**计数值 > 0** |
| 返回 `pdTRUE` / `pdFALSE` | 返回消费前的计数值（0 = 超时） |
| 能读取 32 位通知值及按位清除 | 能将计数清零或减 1 |
| 适合事件标志、传递通知值 | 适合轻量二值/计数信号量 |

### 使用 `xTaskNotifyWait()`

```c
// 接收任务中：阻塞等待，设置进入/退出时不清位，不读取数据
BaseType_t ok = xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(1000));
if (ok == pdTRUE) {
    // 收到通知
} else {
    // 等待超时
}
```

发送方：

```c
xTaskNotify(workerHandle, 0, eNoAction);
```

**细节**：`eNoAction` 仍设置 Pending，但不改通知值。`xTaskNotifyWait()` 的 `ulBitsToClearOnEntry` 只在入口处尚无 Pending 通知时生效；`ulBitsToClearOnExit` 可清除指定通知值位。

事件位示例：

```c
#define EVT_WIFI (1UL << 0)
#define EVT_USB  (1UL << 1)

// 发送端：位或操作，可合并多种事件
xTaskNotify(workerHandle, EVT_WIFI, eSetBits);

// 接收端：收到值时记录后清除全部位
uint32_t events;
if (xTaskNotifyWait(0, 0xFFFFFFFFUL, &events, portMAX_DELAY) == pdTRUE) {
    if (events & EVT_WIFI) { /* WiFi 已就绪 */ }
    if (events & EVT_USB)  { /* USB 有事件 */ }
}
```

### 使用 `ulTaskNotifyTake()`

```c
// 发送端：通知值加 1
xTaskNotifyGive(workerHandle);

// 接收端：每次减 1
uint32_t previousCount = ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

// 或者接收端：一次清零所有累计通知
uint32_t previousCount2 = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

切记：发送端 `eNoAction` 不会递增计数，不能将接收端机械改成 `ulTaskNotifyTake()`。

## `FromISR` API 与中断唤醒

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != GPIO_PIN_0) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    TaskHandle_t target = next_task_handle;
    if (target != NULL) {
        xTaskNotifyFromISR(target, 0, eNoAction,
                           &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
```

- `xHigherPriorityTaskWoken` 必须初始化为 `pdFALSE`。
- `xTaskNotifyFromISR` 在中断中发送通知；若唤醒更高优先级任务，`portYIELD_FROM_ISR` 请求退出中断后调度。
- 在常见 Cortex-M FreeRTOS 移植中，PendSV 负责真正上下文切换。
- NVIC 中断优先级数字越小，硬件紧急程度越高；调用 FreeRTOS `FromISR` API 的中断不能高于 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 允许的紧急程度。
- EXTI 多次抖动可能造成任务被连续通知，实际工程需要按键消抖。

## 三个 LED 同函数、独立任务的完整解释

```c
xTaskCreate(toggle_led_task_handler, "GREEN", 120, &green_pin, 2, &green_led);
xTaskCreate(toggle_led_task_handler, "ORANGE", 120, &orange_pin, 2, &orange_led);
xTaskCreate(toggle_led_task_handler, "RED", 120, &red_pin, 2, &red_led);
next_task_handle = green_led;
```

任务函数中：

```c
Led_t *pin = (Led_t *)params;
for (;;) {
    HAL_GPIO_TogglePin(GPIOD, pin->pin);
    BaseType_t received = xTaskNotifyWait(0, 0, NULL, pin->frequency);
    if (received != pdTRUE) continue;  // 超时则继续闪烁

    // 只有接到通知的任务才继续执行更新目标与自删除
    // Green -> Orange -> Red -> NULL
    vTaskDelete(NULL);                 // 删除当前任务实例
}
```

为什么三个任务执行同一段 switch，却不会同时删除？

1. 三任务各有独立通知状态；中断只通知 `next_task_handle` 指向的任务。
2. 其他任务 `xTaskNotifyWait()` 超时则 `continue`，跳过后面的 switch + 删除。
3. 被通知任务根据自己 `pin->pin` 选择下一目标，然后执行 `vTaskDelete(NULL)`；`NULL` 表示**当前任务实例**。

完整且修正了一些明显问题的示例：[`../examples/02-led-notify-delete.c`](../examples/02-led-notify-delete.c)。

## 临界区与共享句柄

```c
taskENTER_CRITICAL();
next_task_handle = anotherHandle;
taskEXIT_CRITICAL();
```

- 临界区用于短小共享状态更新；不要把 `SEGGER_SYSVIEW_PrintfTarget()` 或耗时外设操作长期放在临界区。
- 与 ISR 共享的变量常声明为 `volatile`，但 `volatile` **不能代替互斥或中断同步**。
- 任务退出前应考虑中断重复通知及句柄有效性；示例增加了空句柄检查，但仍需实际按键消抖。
