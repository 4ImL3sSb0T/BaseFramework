# STM32H750 BaseFramework - 硬件参考

## MCU

- **型号**：STM32H750VBTx
- **内核**：ARM Cortex-M7, 480MHz
- **封装**：LQFP100
- **电源**：LDO (PWR_LDO_SUPPLY)

## 时钟树

| 节点 | 配置 |
|------|------|
| 时钟源 | HSI (64MHz) |
| PLL1M | 4 |
| PLL1N | 60 |
| PLL1P | 2 |
| PLL1Q | 5 |
| PLL1R | 2 |
| SYSCLK | 480MHz (PLL1P) |
| HCLK | 240MHz (÷2) |
| APB1 | 120MHz (÷2) |
| APB2 | 120MHz (÷2) |
| APB3 | 120MHz (÷2) |
| APB4 | 120MHz (÷2) |
| Systick | 来自于 TIM17（非 Cortex Systick），用于 HAL 时基 |

## 外设配置

| 外设 | 实例 | 引脚 | 参数 | 中断/DMA |
|------|------|------|------|----------|
| USART1 | TX: PA9, RX: PA10 | 115200-8-N-1, 无流控, FIFO已禁用 | DMA1_Stream1 (RX, Circular), DMA1_Stream2 (TX, Normal), USART1_IRQn (pri=5) |
| I2C1 | PB6 (SCL), PB7 (SDA) | 标准模式 | — |
| TIM17 | 内部 | HAL 时基（1ms） | TIM17_IRQn → HAL_IncTick() |
| GPIO | PC0 | 红色 LED（推挽输出） | — |
| GPIO | PC1 | 绿色 LED（推挽输出） | — |
| GPIO | PC2 | 蓝色 LED（推挽输出） | — |
| GPIO | PC13 | 用户按键（输入） | — |
| DMA | DMA1_Stream1 | USART1_RX（外设→内存，循环模式） | — |
| DMA | DMA1_Stream2 | USART1_TX（内存→外设，普通模式） | — |

## 引脚总表

| 引脚 | 功能 | 模式 | 备注 |
|------|------|------|------|
| PA9 | USART1_TX | AF7 (推挽) | UART 发送 |
| PA10 | USART1_RX | AF7 (推挽) | UART 接收 |
| PB6 | I2C1_SCL | AF4 (开漏) | I2C 时钟 |
| PB7 | I2C1_SDA | AF4 (开漏) | I2C 数据 |
| PC0 | GPIO_Output | 推挽 | 红色 LED |
| PC1 | GPIO_Output | 推挽 | 绿色 LED |
| PC2 | GPIO_Output | 推挽 | 蓝色 LED |
| PC13 | GPIO_Input | — | 用户按键 |

## MPU 配置

| 区域 | 基址 | 大小 | 访问 | Cache | Buffer | Shareable |
|------|------|------|------|-------|--------|-----------|
| Region0 | 0x00000000 | 4GB | 禁止访问 | No | No | Yes |
| Region1 | 0x30000000 | 32KB | 全访问 | No | No | No |

说明：Region0 作为默认背景区域禁止所有访问，Region1 允许访问 0x30000000 区域。

## HAL 模块

已启用的 HAL 模块：TIM, UART, GPIO, DMA, MDMA, RCC, FLASH, EXTI, PWR, I2C, CORTEX, HSEM

## FreeRTOS

| 参数 | 值 |
|------|-----|
| 版本 | V10.3.1 |
| API | CMSIS-RTOS V2 |
| 调度 | 抢占式, 1kHz tick |
| 优先级 | 56 级 |
| 堆 | heap_4, 64KB |
| 默认任务 | defaultTask, osPriorityNormal, 512B stack |

## 调试接口

- **SWD**：标准 ARM Cortex-M 调试接口
- **SEGGER SystemView**：通过 J-Link RTT 记录 FreeRTOS 运行时事件（已集成）
