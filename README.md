# FreeRTOS 学习笔记（STM32 HAL）

> 主题：任务创建与调度、任务栈与内存、相对/绝对延时、任务通知、EXTI 中断、队列、信号量、互斥锁、事件组与软件定时器。  
> 平台背景：STM32F4/H7 + FreeRTOS；示例大多采用 **原生 FreeRTOS API**，并结合 STM32 HAL。  
> 说明：`examples/` 中是**教学代码片段**，需嵌入已经配置 FreeRTOS 的 STM32CubeMX 工程，并修改 GPIO/中断/时钟配置后才能运行。

## 目录

- [01. FreeRTOS 核心概念](#01-freertos-核心概念)
- [02. API 速查](#02-api-速查)
- [03. 四组最容易混淆的 API](#03-四组最容易混淆的-api)
- [04. 三个 LED + 按键通知 + 自删除案例](#04-三个-led--按键通知--自删除案例)
- [05. 工程注意事项](#05-工程注意事项)
- [06. 阅读和练习顺序](#06-阅读和练习顺序)
- [完整分章笔记](#完整分章笔记)
- [参考资料](#参考资料)

## 01. FreeRTOS 核心概念

1. **任务函数 ≠ 任务实例**：一个入口函数可以创建多个任务；各任务拥有独立的 TCB、栈、局部变量、通知状态、任务句柄和运行进度，而**代码只有一份**。
2. **单核并发 ≠ 并行**：STM32F407 单核一次只执行一个任务；由调度器决定运行哪一个。
3. **任务状态**：运行 Running、就绪 Ready、阻塞 Blocked、挂起 Suspended。阻塞态等待时间/事件，挂起态需显式恢复。
4. **优先级**：FreeRTOS 中任务优先级数值越大通常越高；高优先级就绪任务优先获得 CPU。
5. **任务调度**：SysTick 通常提供 RTOS Tick；PendSV 通常用于上下文切换；SVC 可用于启动第一个任务（取决于移植层）。
6. **任务栈**：任务各自拥有栈。`xTaskCreate(..., usStackDepth, ...)` 中深度的单位是 `StackType_t` 元素；32 位 Cortex-M 上通常每个元素 4 字节。
7. **临界区不等于互斥锁**：临界区通常通过中断屏蔽保护极短操作；跨任务保护耗时外设访问优先使用互斥锁。

## 02. API 速查

### 任务创建/管理（`task.h`）

| API | 用途 | 关键点 |
|---|---|---|
| `xTaskCreate(fn, name, depth, param, priority, &handle)` | 创建动态任务 | `depth` 为栈元素数；任务参数保存的是指针，不会复制结构体 |
| `xTaskCreateStatic(...)` | 静态创建任务 | 自行提供 TCB 和栈存储 |
| `vTaskDelete(NULL)` | 删除当前任务 | 自删除后不会继续执行；动态任务的内核内存由 Idle 任务清理 |
| `vTaskSuspend(handle)` / `vTaskResume(handle)` | 挂起/恢复任务 | 挂起不随时间自动结束 |
| `vTaskPrioritySet()` / `uxTaskPriorityGet()` | 设置/获取优先级 | 尽量根据实时性而非功能重要性设置 |
| `xTaskGetCurrentTaskHandle()` | 获取当前任务句柄 | 句柄用于管理任务，不是入口函数指针 |
| `vTaskStartScheduler()` | 启动调度器 | 正常启动后通常不返回 |

### 调度/延时（`task.h`）

| API | 用途 | 注意事项 |
|---|---|---|
| `vTaskDelay(pdMS_TO_TICKS(100))` | 相对延时 | 从调用时开始等待指定 Tick；任务进入阻塞态 |
| `vTaskDelayUntil(&lastWake, period)` | 绝对周期延时 | 每次更新 `lastWake`，以固定 Tick 基准确定下一周期；超期不会倒退时间 |
| `taskYIELD()` | 请求重新调度 | **不会**进入阻塞态；高优先级任务持续调用仍可能饿死低优先级任务 |
| `xTaskGetTickCount()` | 获取 FreeRTOS Tick | 不等于 `HAL_GetTick()` 的 HAL 时基 |
| `pdMS_TO_TICKS(ms)` | 毫秒转 RTOS Tick | 结果受 `configTICK_RATE_HZ` 和 Tick 粒度影响 |

### 任务通知（`task.h`）

| API | 用途 | 推荐配对 |
|---|---|---|
| `xTaskNotify(handle, value, eAction)` | 任务上下文发送状态/数值通知 | `xTaskNotifyWait()` |
| `xTaskNotifyFromISR(...)` | 中断中发送通知 | `xTaskNotifyWait()` |
| `xTaskNotifyWait(entryMask, exitMask, &value, timeout)` | 等通知状态 Pending | `eNoAction` / `eSetBits` / 带数值通知 |
| `xTaskNotifyGive(handle)` | 任务通知计数 +1 | `ulTaskNotifyTake()` |
| `vTaskNotifyGiveFromISR(handle, &woken)` | ISR 通知计数 +1 | `ulTaskNotifyTake()` |
| `ulTaskNotifyTake(clear, timeout)` | 等待通知计数 > 0 | `pdTRUE` 清零，`pdFALSE` 减 1；返回**消费前计数** |

`xTaskNotifyWait()` 关心**通知状态**是否 Pending；`ulTaskNotifyTake()` 关心**通知值**是否非零。用 `eNoAction` 发送通知不会增加通知值，不能直接依靠它唤醒 `ulTaskNotifyTake()`。

### 通知动作 `eNotifyAction`

| 动作 | 效果 |
|---|---|
| `eNoAction` | 通知状态设为 Pending，不改通知值 |
| `eSetBits` | 将指定的位 OR 到通知值中 |
| `eIncrement` | 通知值加 1 |
| `eSetValueWithOverwrite` | 覆盖通知值 |
| `eSetValueWithoutOverwrite` | 若已有 Pending 通知，则本次写入失败 |

### 队列、信号量、互斥锁、事件组（`queue.h` / `semphr.h` / `event_groups.h`）

| API | 用途 | 注意事项 |
|---|---|---|
| `xQueueCreate(len, itemSize)` | 创建队列 | 每项按 `itemSize` **复制**；存指针时只复制指针 |
| `xQueueSend()` / `xQueueReceive()` | 发送/接收数据 | 中断用对应 `FromISR` 版本 |
| `xSemaphoreCreateBinary()` | 创建二值信号量 | 用于事件同步，初始无令牌 |
| `xSemaphoreCreateCounting(max, initial)` | 创建计数信号量 | 计数资源或事件 |
| `xSemaphoreTake()` / `xSemaphoreGive()` | 获取/释放信号量或互斥锁 | Mutex 需由持有者释放 |
| `xSemaphoreCreateMutex()` | 创建互斥锁 | 有优先级继承；不能在 ISR 中获取/释放 Mutex |
| `xEventGroupCreate()` | 创建事件组 | 多种事件条件组合 |
| `xEventGroupSetBits()` / `xEventGroupWaitBits()` | 设置/等待事件位 | 等待可以选择任意位或全部位；中断设置位请使用对应 `FromISR` API |

### 临界区 / 中断

| API | 用途 |
|---|---|
| `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` | 任务中进入/退出临界区 |
| `portENTER_CRITICAL()` / `portEXIT_CRITICAL()` | 移植层提供的临界区宏，含义随端口变化 |
| `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)` | 必要时请求中断退出后的上下文切换 |
| `configASSERT(condition)` | 检查运行条件，失败行为由项目宏定义决定 |

### 内存、软件定时器与调试

| API | 用途 |
|---|---|
| `pvPortMalloc()` / `vPortFree()` | FreeRTOS 动态内存管理；是否可释放取决于 Heap 实现 |
| `xPortGetFreeHeapSize()` | 当前剩余 FreeRTOS Heap |
| `xPortGetMinimumEverFreeHeapSize()` | 历史最小剩余 Heap |
| `uxTaskGetStackHighWaterMark(handle)` | 历史最小栈余量，单位是 `StackType_t` 元素 |
| `xTimerCreate()` / `xTimerStart()` / `xTimerStop()` / `xTimerReset()` | 软件定时器相关操作 |

软件定时器回调运行在**Timer Service Task** 中，不是硬件中断函数，不能长时间阻塞。

## 03. 四组最容易混淆的 API

### ① `taskYIELD` vs `vTaskDelay`

```c
// 当前任务仍处于就绪状态：低优先级任务不一定有机会运行
taskYIELD();

// 当前任务进入阻塞态：其他就绪任务得到执行机会
vTaskDelay(pdMS_TO_TICKS(100));
```

### ② 相对延时 vs 绝对周期延时

```c
// 相对延时：处理耗时会加入任务周期
while (1) {
    SampleSensor();
    vTaskDelay(pdMS_TO_TICKS(100));
}

// 绝对周期延时：尽量维持 100 ms 周期
TickType_t lastWake = xTaskGetTickCount();
while (1) {
    SampleSensor();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(100));
}
```

### ③ HAL 时基 vs FreeRTOS 时基

- CubeMX 中可选 TIM6 产生 `HAL_IncTick()`，一般每 1 ms 一次；`HAL_GetTick()`、`HAL_Delay()` 使用 HAL 时基。
- Cortex-M FreeRTOS 通常以 SysTick 维护自己的系统 Tick；`xTaskGetTickCount()`、`vTaskDelay()` 使用 RTOS 时基。
- `HAL_Delay(100)` 的单位仍为 **ms**，前提是 HAL Timebase 与 TIM6 周期匹配；它通常忙等，不适合在高优先级任务中做常规延时。
- CMSIS-RTOS2 的 `osDelay(100)` 参数是 **RTOS Tick 数**，只有 Tick 为 1 ms 时才对应约 100 ms。

### ④ `xTaskNotifyWait` vs `ulTaskNotifyTake`

```c
// 只关心“有事件来了”
xTaskNotify(taskHandle, 0, eNoAction);
// 接收任务中：
BaseType_t received = xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);

// 需要通知计数（比如 DMA 完成次数）
xTaskNotifyGive(taskHandle);
// 接收任务中：
uint32_t countBeforeConsume = ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
```

> 上面是两侧代码的对照片段，并不是把发送和接收写在同一个任务函数内。

## 04. 三个 LED + 按键通知 + 自删除案例

完整教学代码见 [`examples/02-led-notify-delete.c`](examples/02-led-notify-delete.c)。

**按键→EXTI ISR→通知 `next_task_handle`→目标任务的 `xTaskNotifyWait()` 返回 `pdTRUE`→更新下一个目标→`vTaskDelete(NULL)`。**

1. Green（LD4）收到通知：`next_task_handle = orange_led`，Green 自删除。
2. Orange（LD3）收到通知：`next_task_handle = red_led`，Orange 自删除。
3. Red（LD5）收到通知：`next_task_handle = NULL`，Red 自删除。

三个任务虽然调用**同一个函数**，但 TCB、栈、局部变量、通知状态、句柄独立。函数主体是 `toggle_led_task_handler()`，三个**任务实例**不是一个任务。

### 原示例中应修正的细节

- `BaseType_t xHigherPriorityTaskWoken = pdFALSE;` 必须先初始化。
- 向任务通知前检查 `next_task_handle != NULL`，否则第三个任务删除后的再按键可能出错。
- 机械按键需要消抖，避免一次按下连续触发多个通知。
- 应确保 EXTI 的 NVIC 优先级**允许调用 FreeRTOS FromISR API**。
- 临界区只包围共享状态更新，避免把 SEGGER 日志输出放进临界区。
- `HAL_GPIO_WritePin(..., GPIO_PIN_SET)` 并不一定代表关灯：STM32F4 Discovery 这类高电平点亮的 LED，SET 反而代表常亮。
- 单核演示里三任务主要用于观察调度；实际三路 LED 也可以由一个任务管理。

## 05. 工程注意事项

- `xTaskCreate()` 任务栈参数在典型 Cortex-M32 上 `120` 表示约 **480 字节**，不是 120 字节；具体以 `StackType_t` 为准。
- FreeRTOS 栈、Heap、`malloc` 不要混为一谈；按实际配置选择 `heap_4` 等内存管理方案。
- 任务通知通常是**针对特定任务**的轻量级机制；多接收者、传输结构体数据更适合队列。
- `xTaskNotifyWait()` 的 `ulBitsToClearOnEntry` 只在调用时**没有通知 Pending**的情况下执行位清除。
- 任务在 `xTaskNotifyWait()` 超时后 `continue`，是返回 `while` 开始，并不会进入自删除流程。
- `vTaskDelete(NULL)` 只删除**当前任务实例**；不删除共享入口函数，也不会删除其他实例。
- 在 STM32H7 DMA/DCMI/LTDC 项目中，除任务同步，还需考虑 DMA 缓冲区所有权与 D-Cache 一致性。

## 06. 阅读和练习顺序

1. 先看 [`docs/01-task-and-tick.md`](docs/01-task-and-tick.md)：任务、栈、调度、延时和 TIM6。
2. 再看 [`docs/02-notification-and-isr.md`](docs/02-notification-and-isr.md)：任务通知与外部中断。
3. 运行 [`examples/02-led-notify-delete.c`](examples/02-led-notify-delete.c) 的改造版，使用 SystemView 看任务唤醒与删除。
4. 看 [`docs/03-ipc-and-debug.md`](docs/03-ipc-and-debug.md)：队列、信号量、互斥锁、事件组、调试。
5. 学习 [`docs/04-github-upload.md`](docs/04-github-upload.md)：把笔记上传 GitHub，后续持续更新。

## 完整分章笔记

- [任务、栈、调度、延时、HAL TIM6](docs/01-task-and-tick.md)
- [任务通知、ISR、中断优先级、LED 项目分析](docs/02-notification-and-isr.md)
- [队列、信号量、互斥锁、事件组、定时器、故障排查](docs/03-ipc-and-debug.md)
- [GitHub 上传步骤（浏览器 / Windows Git）](docs/04-github-upload.md)

## 参考资料

- [FreeRTOS 官方内核源码 `task.h`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/include/task.h)
- [FreeRTOS 官方文档：vTaskDelayUntil](https://freertos.org/Documentation/02-Kernel/04-API-references/02-Task-control/02-vTaskDelayUntil)
- [FreeRTOS 官方文档：ulTaskNotifyTake](https://github.com/FreeRTOS/FreeRTOS-Website-Content/blob/main/content/en-us/Documentation/02-Kernel/04-API-references/05-Direct-to-task-notifications/03-ulTaskNotifyTake.md)
- [FreeRTOS 官方内核开发者手册](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book)
- [GitHub：创建仓库](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository)
- [GitHub：上传文件](https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository)

---

**学习口诀**：函数定义*怎么做*，参数定义*做什么*，调度器决定*什么时候做*；通知等事件，队列传数据，互斥锁保护共享资源。
