# 01 · 任务、调度、栈、延时与 HAL TIM6

## 任务函数与任务实例

```c
static void led_task(void *argument)
{
    const LedConfig *cfg = (const LedConfig *)argument;
    for (;;) {
        HAL_GPIO_TogglePin(GPIOD, cfg->pin);
        vTaskDelay(cfg->periodTicks);
    }
}

// 三次 xTaskCreate()：三个不同任务实例，共用一份入口函数代码
xTaskCreate(led_task, "G", 256, &greenCfg, 2, &greenHandle);
xTaskCreate(led_task, "O", 256, &orangeCfg, 2, &orangeHandle);
xTaskCreate(led_task, "R", 256, &redCfg, 2, &redHandle);
```

这里 `LedConfig`、`greenCfg` 等为示意标识，完整定义可参考 `examples/01-shared-task-function.c`。

- 每个任务独立拥有 **TCB、任务栈、通知状态、执行位置**。
- `cfg` 是各任务独立的局部指针变量，指向相应配置；配置对象本身要保证任务存续期间有效，适合使用 `static` 或全局对象。
- 同一个函数体中的 `BaseType_t status` 也是各任务独立的栈上数据。
- 共享全局变量则仍然需要同步保护。

## `xTaskCreate()` 六个参数

```c
BaseType_t ok = xTaskCreate(
    led_task,       // ① 函数地址
    "G",            // ② 调试任务名
    256,            // ③ 栈深度：StackType_t 元素数
    &greenCfg,      // ④ 任务参数地址
    2,              // ⑤ 任务优先级
    &greenHandle    // ⑥ 保存句柄的变量地址
);
configASSERT(ok == pdPASS);
```

- `&greenCfg` 是传给**任务函数**的数据指针。
- `&greenHandle` 是给 **FreeRTOS 写回任务句柄**的位置。
- `256` 在 32 位 STM32 通常约 1024 字节任务栈，但实际要用水位线检查。

## 任务状态与调度

- Ready：满足运行条件，等待 CPU。
- Running：当前使用 CPU。
- Blocked：等待 Tick 到期 / 通知 / 队列 / 信号量。
- Suspended：显式挂起，需要恢复。

**单核 STM32 上，三个任务不是同时运行。** 在任务切换时内核保存寄存器与栈上下文，然后恢复另一个任务的上下文。FreeRTOS 一般通过 SysTick 维护节拍，通过 PendSV 切换上下文。

`taskYIELD()` 只是请求重新调度，不会使调用任务变为 Blocked。例如高优先级任务一直 `taskYIELD()`，低优先级任务仍可能永远没有机会运行。

## 相对延时和绝对周期延时

```c
// 相对延时
for (;;) {
    ReadSensor();
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

如果 `ReadSensor()` 耗时 20 ms，任务重复周期约为 120 ms。

```c
// 绝对周期延时
TickType_t lastWake = xTaskGetTickCount();
for (;;) {
    ReadSensor();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(100));
}
```

目标执行时间点约为 0、100、200、300 ms（忽略实际调度延迟）。若处理超时，不会强迫任务按过去的时间运行。

## STM32 HAL Tick / FreeRTOS Tick

STM32CubeMX 工程中常见方案：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        HAL_IncTick();
    }
}
```

- TIM6 用作 HAL 时基时通常设为 **1 ms 一次更新**，其周期在 `HAL_InitTick()` 的 timebase 文件中配置。
- `HAL_GetTick()` / `HAL_Delay(100)` 基于 HAL 时基；`HAL_Delay(100)` 仍表示毫秒，前提是 HAL Tick 频率配置正确。
- FreeRTOS 的 Tick 通常由 SysTick 维护；`vTaskDelay(pdMS_TO_TICKS(100))` 在阻塞态等待。
- `HAL_Delay()` 通常忙等，不要在高优先级任务中当作普通任务延时；硬件初始化阶段的使用应检查中断与时基配置。
- `osDelay()` 属 CMSIS-RTOS 接口，不要和原生 FreeRTOS 的 `vTaskDelay()` 混用概念；CMSIS-RTOS2 `osDelay()` 参数单位是 Tick。

## 栈和 Heap

- Task Stack：局部变量、函数调用现场、上下文等。
- FreeRTOS Heap：动态创建 TCB、任务栈、队列等的内存来源（取决于使用何种 API 和 Heap 实现）。
- 示例：`xTaskCreate(..., 120, ...)` 在常见 32 位 STM32 上约对应 480 字节栈空间。
- 运行中检查：

```c
UBaseType_t freeWords = uxTaskGetStackHighWaterMark(NULL);
size_t freeHeap = xPortGetFreeHeapSize();
size_t minHeap = xPortGetMinimumEverFreeHeapSize();
```

开启 `configCHECK_FOR_STACK_OVERFLOW`、定义 `vApplicationStackOverflowHook`、启用 `configASSERT` 能辅助定位问题，但不替代实际测试与内存边界检查。
