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
| SPI1 | PB3 (SCK), PB5 (MOSI) | Master, **PLL1Q÷2**（Cube `PRESCALER_2`，约 48MHz 量级，以实测稳定为准）, CPOL=0 CPHA=1Edge (Mode0), MSB, 8-bit, 仅发送(1-Line), 软件 NSS；**TX DMA** = DMA1_Stream2 | 外接 ST7735 TFT LCD |
| SPI2 | PB13 (SCK), PB14 (MISO), PB15 (MOSI), PB12 (CS) | Master, 48 Mbps, CPOL=1 CPHA=1, MSB, 8-bit, 软件 NSS | 外接 SPI Flash，CS 由 PB12 GPIO 控制 |
| ADC1 | PA6 (INP3), PA7 (INP7), PB1 (INP5) | 12-bit、单端、扫描 **3** 通道、连续转换、软件触发；V/I 采样 64.5 cycles，温度 387.5 cycles；DMA 循环 | DMA2_Stream0 (ADC1, Circular, halfword)；应用层 `bsp_adc` / `sense` |
| DAC1 | PA4 (`LOADER_REF`) | CH1，12-bit，软件触发，输出缓冲关闭，工厂 trim | 电子负载电流/功率级基准；应用层经 `bsp_dac` / `load_out` 驱动 |
| TIM15 | PE5 (`FUN_PWM` / 风扇) | PWM CH1；Cube PSC=240，ARR 在 `bsp_fan_init` 改为 39（约 25 kHz） | 风扇驱动；`bsp_fan` / `service/fan` |
| TIM17 | 内部 | HAL 时基（1ms） | TIM17_IRQn → HAL_IncTick() |
| GPIO | PC0 | 红色 LED（推挽输出） | — |
| GPIO | PC1 | 绿色 LED（推挽输出） | — |
| GPIO | PC2 | 蓝色 LED（推挽输出） | — |
| GPIO | PC13 | 用户按键 USR_KEY（上拉输入，按下低） | — |
| GPIO | PD15 | UI_UP（上拉输入，按下低） | multi_button / loader_ui |
| GPIO | PD14 | UI_DOWN（上拉输入，按下低） | multi_button / loader_ui |
| GPIO | PD13 | UI_ENT（上拉输入，按下低） | multi_button / loader_ui |
| GPIO | PD12 | UI_BACK（上拉输入，按下低） | multi_button / loader_ui |
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
| PB1 | ADC1_INP5 / `LOADER_TEMP` | 模拟输入 | 负载温度采样（Rank3） |
| PE5 | TIM15_CH1 / `FUN_PWM` | AF4 PWM | 风扇 PWM 输出 |
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
| PC13 | GPIO_Input | 上拉 | 用户按键 USR_KEY（按下低） |
| PD12 | GPIO_Input | 上拉 | UI_BACK（按下低） |
| PD13 | GPIO_Input | 上拉 | UI_ENT（按下低） |
| PD14 | GPIO_Input | 上拉 | UI_DOWN（按下低） |
| PD15 | GPIO_Input | 上拉 | UI_UP（按下低） |
| PB12 | GPIO_Output | 推挽 | SPI Flash CS |
| PB13 | SPI2_SCK | AF5 (推挽) | SPI Flash 时钟 |
| PB14 | SPI2_MISO | AF5 (推挽) | SPI Flash 数据输入 |
| PB15 | SPI2_MOSI | AF5 (推挽) | SPI Flash 数据输出 |

## 启动与 XIP

本应用 **不直接从内部 Flash 启动**。内部 Flash `0x08000000` 存放 **Bootloader**（初始化 QSPI Memory Map 后跳转）；本工程链接并运行在外部 Flash XIP 区。

| 部分 | 地址 | 说明 |
|------|------|------|
| Bootloader | `0x08000000` 128 KB 内部 Flash | 映射 W25Q64 → 跳转 App |
| App（本工程） | `0x90000000` 最多 8 MB QSPI XIP | 代码 + RO；VTOR = `APPLICATION_ADDRESS` |
| 数据 SPI Flash（SFUD） | SPI2，与 QSPI 独立 | 文件系统等，非 XIP |

- Keil 宏：`APPLICATION_ADDRESS=0x90000000U`
- VTOR：`Core/Src/system_stm32h7xx.c`（`USER_VECT_TAB_ADDRESS` → `0x90000000`）
- 下载：使用外部 Flash FLM 算法烧写到 `0x90000000`（勿覆盖内部 Bootloader）
- **禁止**在 App 中重新初始化 QSPI，或改动 QSPI 引脚（否则 XIP 立即失效）

## MPU 配置

| 区域 | 基址 | 大小 | 访问 | Cache | Buffer | Shareable | 可执行 |
|------|------|------|------|-------|--------|-----------|--------|
| Region0 | 0x00000000 | 4GB | 禁止访问 | No | No | Yes | No |
| Region1 | 0x30000000 | 64KB | 全访问 | No | No | No | No |
| Region2 | 0x90000000 | 8MB | 全访问 | Yes | Yes | No | **Yes** |

说明：Region0 为背景禁访区（SubRegionDisable=`0x87`，其中含 `0x80000000–0x9FFFFFFF`）。Region1 将 D2 SRAM1 前 64KB 设为 **non-cacheable**（DMA）。Region2 覆盖 QSPI XIP，覆盖背景禁访并允许取指 + Cache。

### 链接内存布局（`MDK-ARM/BaseFramework.sct`）

| 区域 | 基址 | 大小 | 内容 | Cache / 备注 |
|------|------|------|------|----------------|
| QSPI XIP | `0x90000000` | 8 MB | 代码 + RO（外部 W25Q64） | MPU Region2 Cacheable |
| 内部 Flash | `0x08000000` | 128 KB | **仅 Bootloader**，本工程不链接 | — |
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

## ADC1（电压 / 电流 / 温度采样）

用于电子负载测量通道，CubeMX 已生成 `MX_ADC1_Init()`；应用层见 `src/bsp/adc`、`src/service/sense`。

| 参数 | 值 |
|------|-----|
| 实例 | ADC1，独立模式 |
| 分辨率 | 12-bit（满量程 raw 0..4095） |
| 输入 | 单端 |
| 扫描 | 使能，`NbrOfConversion=3` |
| 连续转换 | 使能 |
| 触发 | 软件触发（`ADC_SOFTWARE_START`） |
| 数据路径 | `ADC_CONVERSIONDATA_DMA_CIRCULAR` + DMA 循环 |
| 过载策略 | `ADC_OVR_DATA_OVERWRITTEN` |
| 采样时间 | V/I：`64.5` cycles；温度：`387.5` cycles |
| 时钟 | 源 PLL2P=80MHz，分频 `ADC_CLOCK_ASYNC_DIV4` → fADC≈20MHz |
| DMA | DMA2_Stream0，`DMA_REQUEST_ADC1`，外设→内存，半字 |
| 应用缓冲 | `adc_com_buffer[3]` 于 `.dma_buf`：`[0]=V，[1]=I，[2]=Temp` |

| Rank | 通道 | 引脚 | Cube 标签 | 用途 |
|------|------|------|-----------|------|
| 1 | ADC_CHANNEL_3 (INP3) | PA6 | `Voltage_CH` | 电压采样 |
| 2 | ADC_CHANNEL_7 (INP7) | PA7 | `Current_CH` | 电流采样 |
| 3 | ADC_CHANNEL_5 (INP5) | PB1 | `LOADER_TEMP` | 温度采样 |

**温度换算（service）**

- `T(°C) = raw * SENSE_TEMP_FACTOR`（默认 `0.1`，按传感器标定改 `sense.h`）
- 不经过「码值→电压→温度」二次换算，与当前硬件约定一致

**使用注意**

1. `bsp_adc_init()` 内做校准 + `HAL_ADC_Start_DMA`。
2. DMA 缓冲必须 `.dma_buf`（D-Cache 开启）。
3. V/I：`Vphys = (raw/4095)*3.3 * FACTOR`，系数在 `sense.h`。

## TIM15（风扇 PWM）

| 参数 | 值 |
|------|-----|
| 实例 | TIM15 CH1 |
| 引脚 | PE5（Cube 标签 `FUN_PWM`） |
| Cube PSC | 240 |
| 应用 ARR | `bsp_fan_init` 设为 39 → 约 **25 kHz** |
| 占空比 | `bsp_fan_set_duty(0..1)` / `fan_set_speed` / `fan_set_percent` |

上层优先用 `service/fan`（enable + 目标转速）；禁止 UI 直接写 TIM 寄存器。

## SFUD (Serial Flash Universal Driver Library)

| 参数 | 值 |
|------|-----|
| 版本 | v1.1.0 |
| 接口 | SPI2 |
| 设备表索引 | SFUD_W25QXX_DEVICE_INDEX = 0 |
| SFDP 支持 | 启用（自动检测 Flash 参数） |
| QSPI | XIP 由 Bootloader 映射到 `0x90000000`（本 App 不初始化 QSPI 外设） |
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
| SPI 时钟 | Cube `SPI_BAUDRATEPRESCALER_2`（源 PLL1Q≈96MHz → 约 48MHz；以示波器/显示稳定性为准） |
| SPI 模式 | Mode0 (CPOL=0, CPHA=1Edge), MSB |
| SPI TX DMA | DMA1_Stream2 / `DMA_REQUEST_SPI1_TX`；大块像素经 `HAL_SPI_Transmit_DMA` |
| 渲染方式 | 离屏 FB RGB565 LE（AXI）；推送前换端序到 `.dma_buf` 暂存区再 DMA |
| 刷新方式 | 先画到 buffer，再 `tft_fb_flush()` / `tft_fb_flush_rect()`（整区一次 DMA） |
| Adafruit 驱动 | ST7735 → ST77xx → SPITFT → GFX；应用层经 `src/bsp/tft_port/tft_fb.*` / `SpiWrapper` |
