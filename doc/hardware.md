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
| SPI1 | PB3 (SCK), PB5 (MOSI) | Master, 6MHz (PLL1Q=96MHz÷16), CPOL=0 CPHA=1Edge (Mode0), MSB, 8-bit, 仅发送(1-Line), 软件 NSS | 外接 ST7735 TFT LCD |
| SPI2 | PB13 (SCK), PB14 (MISO), PB15 (MOSI), PB12 (CS) | Master, 48 Mbps, CPOL=1 CPHA=1, MSB, 8-bit, 软件 NSS | 外接 SPI Flash，CS 由 PB12 GPIO 控制 |
| TIM17 | 内部 | HAL 时基（1ms） | TIM17_IRQn → HAL_IncTick() |
| GPIO | PC0 | 红色 LED（推挽输出） | — |
| GPIO | PC1 | 绿色 LED（推挽输出） | — |
| GPIO | PC2 | 蓝色 LED（推挽输出） | — |
| GPIO | PC13 | 用户按键（输入） | — |
| GPIO | PB4 | TFT DC（推挽输出） | ST7735 数据/命令选择 |
| GPIO | PB6 | TFT CS（推挽输出） | ST7735 片选 |
| GPIO | PB7 | TFT RST（推挽输出） | ST7735 复位 |
| DMA | DMA1_Stream1 | USART1_RX（外设→内存，循环模式） | — |
| DMA | DMA1_Stream2 | USART1_TX（内存→外设，普通模式） | — |

## 引脚总表

| 引脚 | 功能 | 模式 | 备注 |
|------|------|------|------|
| PA9 | USART1_TX | AF7 (推挽) | UART 发送 |
| PA10 | USART1_RX | AF7 (推挽) | UART 接收 |
| PB3 | SPI1_SCK | AF5 (推挽) | ST7735 TFT 时钟 |
| PB4 | GPIO_Output | 推挽 | ST7735 TFT DC (数据/命令) |
| PB5 | SPI1_MOSI | AF5 (推挽) | ST7735 TFT 数据 |
| PB6 | GPIO_Output | 推挽 | ST7735 TFT CS (片选) |
| PB7 | GPIO_Output | 推挽 | ST7735 TFT RST (复位) |
| PC0 | GPIO_Output | 推挽 | 红色 LED |
| PC1 | GPIO_Output | 推挽 | 绿色 LED |
| PC2 | GPIO_Output | 推挽 | 蓝色 LED |
| PC13 | GPIO_Input | — | 用户按键 |
| PB12 | GPIO_Output | 推挽 | SPI Flash CS |
| PB13 | SPI2_SCK | AF5 (推挽) | SPI Flash 时钟 |
| PB14 | SPI2_MISO | AF5 (推挽) | SPI Flash 数据输入 |
| PB15 | SPI2_MOSI | AF5 (推挽) | SPI Flash 数据输出 |

## MPU 配置

| 区域 | 基址 | 大小 | 访问 | Cache | Buffer | Shareable |
|------|------|------|------|-------|--------|-----------|
| Region0 | 0x00000000 | 4GB | 禁止访问 | No | No | Yes |
| Region1 | 0x30000000 | 64KB | 全访问 | No | No | No |

说明：Region0 作为默认背景区域禁止所有访问，Region1 将 D2 SRAM1 前 64KB（`0x30000000`）设为 **non-cacheable**，专供 DMA 缓冲。

### 链接内存布局（`MDK-ARM/BaseFramework.sct`）

| 区域 | 基址 | 大小 | 内容 | Cache / 备注 |
|------|------|------|------|----------------|
| Flash | `0x08000000` | 128 KB | 代码 + RO | — |
| DTCM | `0x20000000` | 128 KB | FreeRTOS heap 112KB + 主栈 16KB | 不走 D-Cache；**DMA 不可访问** |
| AXI SRAM | `0x24000000` | 512 KB | `.data` / `.bss` | 可 Cache |
| D2 SRAM1 | `0x30000000` | 64 KB | `.dma_buf`（如 USART1 DMA） | MPU Region1 不可 Cache |

**DTCM 内部（低→高，必须拆成两个 execution region）**

| 地址 | 大小 | 用途 |
|------|------|------|
| `0x20000000` | 112 KB | FreeRTOS `ucHeap`（`.dtcm_heap` / `RW_DTCM_HEAP`） |
| `0x2001C000` | 16 KB | 主栈 MSP（`STACK` / `RW_DTCM_STACK`），`__initial_sp = 0x20020000` |

STACK 与 heap **不能**放在同一 scatter 执行区：启动 scatter-load 清 ZI 时会把正在用的主栈清掉，导致进不了 `main`。  
C 库 `Heap_Size=0x400` 在 AXI；任务栈 / 队列从 FreeRTOS heap（DTCM）分配。

工程已启用自定义 scatter（`useFile=1`）。新增 DMA buffer：`__attribute__((section(".dma_buf"), aligned(32)))`，勿放 DTCM。

## HAL 模块

已启用的 HAL 模块：TIM, UART, GPIO, DMA, MDMA, RCC, FLASH, EXTI, PWR, I2C, SPI, CORTEX, HSEM

## SFUD (Serial Flash Universal Driver Library)

| 参数 | 值 |
|------|-----|
| 版本 | v1.1.0 |
| 接口 | SPI2 |
| 设备表索引 | SFUD_W25QXX_DEVICE_INDEX = 0 |
| SFDP 支持 | 启用（自动检测 Flash 参数） |
| QSPI | 未启用 |
| retry.times | 10000 |
| 调试输出 | SEGGER RTT (channel 0) |

## FreeRTOS

| 参数 | 值 |
|------|-----|
| 版本 | V10.3.1 |
| API | CMSIS-RTOS V2 |
| 调度 | 抢占式, 1kHz tick |
| 优先级 | 56 级 |
| 堆 | heap_4, 112KB（DTCM `.dtcm_heap`）；主栈 16KB（DTCM） |
| 默认任务 | defaultTask, osPriorityNormal, 512B stack |

## 调试接口

- **SWD**：标准 ARM Cortex-M 调试接口
- **SEGGER SystemView**：通过 J-Link RTT 记录 FreeRTOS 运行时事件（已集成）

## ST7735 TFT LCD 显示

| 参数 | 值 |
|------|-----|
| 型号 | ST7735 (128×160) |
| 接口 | SPI1, 仅发送 (1-Line) |
| SPI 引脚 | PB3=SCK, PB5=MOSI |
| CS 引脚 | PB6 (GPIO 软件控制) |
| DC 引脚 | PB4 (GPIO 软件控制) |
| RST 引脚 | PB7 (GPIO 软件控制) |
| SPI 时钟 | 6MHz (PLL1Q=96MHz ÷ 16) |
| SPI 模式 | Mode0 (CPOL=0, CPHA=1Edge), MSB |
| 渲染方式 | 离屏帧缓冲 RGB565（`tft_fb`，最大 128×160×2 ≈ 40KB 静态区） |
| 刷新方式 | 先画到 buffer，再 `tft_fb_flush()` / `tft_fb_flush_rect()` 批量 SPI 推送 |
| Adafruit 驱动 | ST7735 → ST77xx → SPITFT → GFX；应用层经 `src/bsp/tft_port/tft_fb.*` |
