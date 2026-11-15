# 03 · 队列、信号量、互斥锁、事件组与调试

## 任务通信选择

| 需求 | 推荐机制 |
|---|---|
| 传递 ADC 数值、结构体数据 | Queue |
| ISR 只通知某个任务处理事件 | Task Notification / Binary Semaphore |
| 累计多次事件、表示 N 个资源令牌 | Counting Semaphore / Counting Task Notification |
| SPI、I2C、文件系统共享总线/资源 | Mutex |
| WiFi、SD 卡、LCD 等多个初始化条件 | Event Group |
| 周期性软回调 | Software Timer |

## Queue：按元素大小复制数据

```c
QueueHandle_t adcQueue = xQueueCreate(8, sizeof(uint16_t));
configASSERT(adcQueue != NULL);

// 发送任务
uint16_t adc = 1234;
xQueueSend(adcQueue, &adc, 0);

// 接收任务
uint16_t received;
if (xQueueReceive(adcQueue, &received, portMAX_DELAY) == pdPASS) {
    // received == 1234
}
```

`xQueueSend` 会复制 `sizeof(uint16_t)` 字节。如果队列元素是指针，复制的只有指针值，**必须自己保证指针所指向缓冲区的生命周期**。

## 二值信号量：事件同步

```c
SemaphoreHandle_t doneSem = xSemaphoreCreateBinary();
configASSERT(doneSem != NULL);

// 任务等待
if (xSemaphoreTake(doneSem, portMAX_DELAY) == pdTRUE) {
    // DMA 完成后的业务处理
}

// DMA 中断/回调
BaseType_t woken = pdFALSE;
xSemaphoreGiveFromISR(doneSem, &woken);
portYIELD_FROM_ISR(woken);
```

需要确保对应中断的 NVIC 优先级允许调用 FreeRTOS API。

## Mutex：保护共享资源

```c
SemaphoreHandle_t spiMutex = xSemaphoreCreateMutex();
configASSERT(spiMutex != NULL);

if (xSemaphoreTake(spiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // SPI_Transaction();
    xSemaphoreGive(spiMutex);
}
```

互斥锁有**优先级继承**机制，可缓解优先级反转；与二值信号量不同，Mutex 有所有权语义，**不可在中断中操作**，并须由获取它的任务释放。

## Event Group：等待多个条件

```c
#define EVT_WIFI_READY (1U << 0)
#define EVT_SD_READY   (1U << 1)

EventGroupHandle_t flags = xEventGroupCreate();
configASSERT(flags != NULL);

// 条件满足时：
xEventGroupSetBits(flags, EVT_WIFI_READY);
xEventGroupSetBits(flags, EVT_SD_READY);

// 等待两个条件都满足，不自动清除，最长等待 2 秒
EventBits_t bits = xEventGroupWaitBits(
    flags,
    EVT_WIFI_READY | EVT_SD_READY,
    pdFALSE, pdTRUE,
    pdMS_TO_TICKS(2000)
);

if ((bits & (EVT_WIFI_READY | EVT_SD_READY)) == (EVT_WIFI_READY | EVT_SD_READY)) {
    // 均已准备好
}
```

## 软件定时器

```c
static void TimerCallback(TimerHandle_t timer)
{
    (void)timer;
    // 只做快速、非阻塞工作
}

TimerHandle_t timer = xTimerCreate(
    "periodic", pdMS_TO_TICKS(1000), pdTRUE, NULL, TimerCallback
);
configASSERT(timer != NULL);
xTimerStart(timer, 0);
```

软件定时器回调在 **Timer Service Task** 中执行，不是 TIM 中断；不要在回调中调用可能长时间阻塞的操作。

## 常见故障排查

| 现象 | 优先检查 |
|---|---|
| 无法创建任务 | Heap、栈深度、`xTaskCreate` 返回值 |
| 偶发 HardFault/显示异常 | 栈溢出、缓冲区越界、DMA/D-Cache 同步 |
| 低优先级任务不执行 | 高优先级任务一直 Ready/Running；`taskYIELD()` 不会让它 Blocked |
| EXTI 通知后任务不运行 | `FromISR`、NVIC 优先级、句柄、通知状态、是否请求切换 |
| LED 一次按键删除多个任务 | 机械按键抖动、通知发送过快 |
| 多任务抢同一 SPI | 是否用 Mutex，是否发生死锁 |
| LVGL 控件异常 | 多任务同时操作 LVGL、锁、图像缓冲区所有权 |

推荐打开 `configASSERT`、`configCHECK_FOR_STACK_OVERFLOW`，用 `uxTaskGetStackHighWaterMark()` 和 SEGGER SystemView 辅助观察任务。
