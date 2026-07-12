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
| PLL2 | M=4, N=10, P/Q/R=2；VCO Medium，输入 Range3（8–16MHz） |
| PLL2P | 80MHz（作 ADC 内核时钟源） |
| ADC 内核时钟 | PLL2P / ASYNC_DIV4 ≈ **20MHz** |
| Systick | 来自于 TIM17（非 Cortex Systick），用于 HAL 时基 |

## 外设配置

| 外设 | 实例 | 引脚 | 参数 | 中断/DMA |
|------|------|------|------|----------|
| USART1 | TX: PA9, RX: PA10 | 115200-8-N-1, 无流控, FIFO已禁用 | DMA1_Stream1 (RX, Circular), DMA1_Stream2 (TX, Normal), USART1_IRQn (pri=5) |
| SPI1 | PB3 (SCK), PB5 (MOSI) | Master, 6MHz (PLL1Q=96MHz÷16), CPOL=0 CPHA=1Edge (Mode0), MSB, 8-bit, 仅发送(1-Line), 软件 NSS | 外接 ST7735 TFT LCD |
| SPI2 | PB13 (SCK), PB14 (MISO), PB15 (MOSI), PB12 (CS) | Master, 48 Mbps, CPOL=1 CPHA=1, MSB, 8-bit, 软件 NSS | 外接 SPI Flash，CS 由 PB12 GPIO 控制 |
| ADC1 | PA6 (INP3), PA7 (INP7) | 12-bit、单端、扫描 2 通道、连续转换、软件触发；采样 64.5 cycles；DMA 循环写缓冲 | DMA2_Stream0 (ADC1, Circular, halfword, pri=MEDIUM)；DMA2_Stream0_IRQn (NVIC pri=0) |
| DAC1 | PA4 (`LOADER_REF`) | CH1，12-bit，软件触发，输出缓冲关闭，工厂 trim | 电子负载电流/功率级基准；应用层经 `bsp_dac` / `load_out` 驱动 |
| TIM17 | 内部 | HAL 时基（1ms） | TIM17_IRQn → HAL_IncTick() |
| GPIO | PC0 | 红色 LED（推挽输出） | — |
| GPIO | PC1 | 绿色 LED（推挽输出） | — |
| GPIO | PC2 | 蓝色 LED（推挽输出） | — |
| GPIO | PC13 | 用户按键（输入） | — |
| GPIO | PB4 | TFT DC（推挽输出） | ST7735 数据/命令选择 |
| GPIO | PB6 | TFT CS（推挽输出） | ST7735 片选 |
| GPIO | PB7 | TFT RST（推挽输出） | ST7735 复位 |
| DMA | DMA1_Stream1 | USART1_RX（外设→内存，循环模式） | NVIC pri=5 |
| DMA | DMA1_Stream2 | USART1_TX（内存→外设，普通模式） | NVIC pri=5 |
| DMA | DMA2_Stream0 | ADC1（外设→内存，循环模式，半字） | NVIC pri=0 |

## 引脚总表

| 引脚 | 功能 | 模式 | 备注 |
|------|------|------|------|
| PA4 | DAC1_OUT1 / `LOADER_REF` | 模拟输出 | 电子负载基准电压 |
| PA5 | `LOADER_FAULT` | EXTI 上升沿，下拉 | 负载故障输入 |
| PA6 | ADC1_INP3 / `Voltage_CH` | 模拟输入 | 电压采样通道（Rank1） |
| PA7 | ADC1_INP7 / `Current_CH` | 模拟输入 | 电流采样通道（Rank2） |
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
| D2 SRAM1 | `0x30000000` | 64 KB | `.dma_buf`（USART1 / ADC1 等 DMA） | MPU Region1 不可 Cache |

**DTCM 内部（低→高，必须拆成两个 execution region）**

| 地址 | 大小 | 用途 |
|------|------|------|
| `0x20000000` | 112 KB | FreeRTOS `ucHeap`（`.dtcm_heap` / `RW_DTCM_HEAP`） |
| `0x2001C000` | 16 KB | 主栈 MSP（`STACK` / `RW_DTCM_STACK`），`__initial_sp = 0x20020000` |

STACK 与 heap **不能**放在同一 scatter 执行区：启动 scatter-load 清 ZI 时会把正在用的主栈清掉，导致进不了 `main`。  
C 库 `Heap_Size=0x400` 在 AXI；任务栈 / 队列从 FreeRTOS heap（DTCM）分配。

工程已启用自定义 scatter（`useFile=1`）。新增 DMA buffer：`__attribute__((section(".dma_buf"), aligned(32)))`，勿放 DTCM（ADC/USART 缓冲均须落在此区，因 D-Cache 已开）。

## HAL 模块

已启用的 HAL 模块：ADC, DAC, TIM, UART, GPIO, DMA, MDMA, RCC, FLASH, EXTI, PWR, I2C, SPI, OPAMP, CORTEX, HSEM

## DAC1（负载基准 LOADER_REF）

CubeMX 已生成 `MX_DAC1_Init()`；应用层启动与码值写入见 `src/bsp/dac`、`src/service/load_out`。

| 参数 | 值 |
|------|-----|
| 实例 | DAC1，通道 1 |
| 引脚 | PA4（`LOADER_REF`） |
| 分辨率 | 12-bit，右对齐 `DAC_ALIGN_12B_R` |
| 触发 | 软件触发（`DAC_TRIGGER_SOFTWARE`） |
| 输出缓冲 | 关闭（`DAC_OUTPUTBUFFER_DISABLE`） |
| 片上连接 | 关闭 |
| Trim | 工厂（`DAC_TRIMMING_FACTORY`） |
| 参考 | VDDA（默认按 3.3 V 标定，见 `LOAD_OUT_VREF`） |

**使用注意**

1. `MX_DAC1_Init()` 只配置外设，**不**自动 Start；须调用 `bsp_dac_init()` / `load_out_init()`。
2. 软件触发模式下，每次改码后需 SWTRIG（`bsp_dac_set_raw` 内已处理）。
3. 上层优先使用 `load_out_set(out_norm)`（`out_norm ∈ [0,1]`）；控制环在 `loader_core` 中写执行器，禁止 UI/CLI 直接调 HAL。

## ADC1（电压 / 电流采样）

用于电子负载测量通道，CubeMX 已生成 `MX_ADC1_Init()`；应用层启动转换与物理量换算另见 `src/bsp` / `src/service`。

| 参数 | 值 |
|------|-----|
| 实例 | ADC1，独立模式 |
| 分辨率 | 12-bit |
| 输入 | 单端 |
| 扫描 | 使能，`NbrOfConversion=2` |
| 连续转换 | 使能 |
| 触发 | 软件触发（`ADC_SOFTWARE_START`） |
| 数据路径 | `ADC_CONVERSIONDATA_DMA_CIRCULAR` + DMA 循环 |
| 过载策略 | `ADC_OVR_DATA_OVERWRITTEN` |
| 采样时间 | 两通道均为 `ADC_SAMPLETIME_64CYCLES_5` |
| 时钟 | 源 PLL2P=80MHz，分频 `ADC_CLOCK_ASYNC_DIV4` → fADC≈20MHz |
| DMA | DMA2_Stream0，`DMA_REQUEST_ADC1`，外设→内存，半字，优先级 MEDIUM |
| DMA 中断 | `DMA2_Stream0_IRQn`，NVIC 优先级 0（**若回调内使用 FreeRTOS API，须改为 ≥5**） |

| Rank | 通道 | 引脚 | Cube 标签 | 用途 |
|------|------|------|-----------|------|
| 1 | ADC_CHANNEL_3 (INP3) | PA6 | `Voltage_CH` | 电压采样 |
| 2 | ADC_CHANNEL_7 (INP7) | PA7 | `Current_CH` | 电流采样 |

**使用注意**

1. 启动转换前建议调用 `HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED)`。
2. DMA 缓冲须放在 `.dma_buf`（D2 SRAM，non-cacheable），例如 `uint16_t adc_dma_buf[2]` → `[0]=电压 raw，[1]=电流 raw`。
3. 使用 `HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, 2)` 启动；当前工程默认仅初始化外设，**未**自动 Start。
4. 粗算吞吐：约 77 ADC 周期/通道 @ 20MHz，双通道一轮约 7.7µs（约 130k 次/秒/通道量级），足够负载控制环使用。

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
