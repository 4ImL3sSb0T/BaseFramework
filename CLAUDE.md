# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

STM32H750VBTx (Cortex-M7) 嵌入式基础框架，基于 STM32CubeMX 生成，使用 FreeRTOS (CMSIS-RTOS V2) + HAL 驱动 + Keil MDK-ARM 编译。

## 编译

使用 `/keil-build` skill：
- **编译**：Keil UV4.exe `-b` 参数，输出到 `MDK-ARM/build_output.txt`
- **下载**：Keil UV4.exe `-f` 参数（需用户明确指令）
- 编译器和工程路径定义在 `.claude/skills/keil-build/SKILL.md` 中
- 必须使用 PowerShell 工具执行（Keil 命令行不支持 Bash）

## CubeMX 兼容性（关键约束）

本项目由 CubeMX 生成，所有 CubeMX 管理的文件都有 `USER CODE BEGIN/END` 保护标记：

- **CubeMX 会覆盖的文件**（修改必须放在 USER CODE 块内）：
  - `Core/Src/main.c`、`Core/Src/freertos.c`、`Core/Src/usart.c` 等所有 `Core/Src/*.c`
  - `Core/Inc/main.h`、`Core/Inc/usart.h` 等所有 `Core/Inc/*.h`
  - `Core/Inc/FreeRTOSConfig.h`
  - `Core/Inc/stm32h7xx_hal_conf.h`
- **CubeMX 会整体覆盖的文件**（修改后需手动恢复）：
  - `MDK-ARM/BaseFramework.uvprojx` — 工程文件（添加 SEGGER 源文件组和 include 路径）
  - `MDK-ARM/BaseFramework.uvoptx`
- **用户自由文件**（CubeMX 不触碰）：
  - `Core/Inc/SEGGER/`、`Core/Src/SEGGER/` — 手动添加的 SEGGER SystemView
  - `src/` — 用户应用层代码目录（`app/`、`bsp/`、`driver/`、`service/`）

## 代码架构

### 硬件层
- **MCU**：STM32H750VBTx (Cortex-M7, 480MHz via PLL from HSI 64MHz)
- **时钟**：HSI → PLL (M=4, N=60, P=2) → 480MHz SYSCLK; HCLK=240MHz (÷2)
- **外设**：USART1 (PA9/PA10, 115200-8-N-1, DMA1 Stream1 RX + Stream2 TX), I2C, GPIO (PC0/PC1/PC2 LED, PC13 Key), TIM17 (HAL timebase)
- **MPU**：Region0 全地址无访问权限 + Region1 0x30000000 32KB 全访问
- **HAL 模块已启用**：TIM, UART, GPIO, DMA, MDMA, RCC, FLASH, EXTI, PWR, I2C, CORTEX, HSEM

### FreeRTOS 层
- **FreeRTOS V10.3.1**，通过 CMSIS-RTOS V2 API 使用 (`cmsis_os2.c`)
- **默认配置**：抢占式、heap_4、56 优先级、1kHz tick、堆大小 64KB
- **默认任务**：`defaultTask`（`osPriorityNormal`, 512B stack），定义在 `Core/Src/freertos.c`
- **SEGGER SystemView** 已集成（trace 宏通过 `SEGGER_SYSVIEW_FreeRTOS.h` 注入），文件位于 `Core/Inc/SEGGER/` 和 `Core/Src/SEGGER/`

### 目录结构
```
Core/           ← CubeMX 管理：HAL 配置、FreeRTOSConfig、外设初始化、SEGGER
Drivers/        ← 只读：STM32H7 HAL + CMSIS
Middlewares/    ← 只读：FreeRTOS 内核源码
MDK-ARM/        ← Keil 工程、启动文件、编译输出
src/            ← 用户应用层（分层：app/ bsp/ driver/ service/）
BaseFramework.ioc  ← CubeMX 项目配置（跟踪）
```

## 常用外设引脚

| 功能 | 引脚 | 端口 |
|------|------|------|
| USART1 TX | PA9 | GPIOA |
| USART1 RX | PA10 | GPIOA |
| 红色 LED | PC0 | GPIOC |
| 绿色 LED | PC1 | GPIOC |
| 蓝色 LED | PC2 | GPIOC |
| 用户按键 | PC13 | GPIOC |

## .gitignore 注意事项

不从版本控制中排除的 CubeMX/IDE 文件：
- `.uvprojx`、`.uvoptx` — Keil 工程文件（被 CubeMX 覆盖后需手动恢复）
- `.ioc` — CubeMX 项目配置
- `MDK-ARM/startup_stm32h750xx.s` — 启动文件

已从跟踪中移除（`.gitignore` 已覆盖）：
- `.mxproject`、`MDK-ARM/RTE/`、`MDK-ARM/DebugConfig/` — CubeMX 自动生成
- `MDK-ARM/build_output.txt` — 编译日志
- `.codegraph/`、`.jj/`、`.claude/` — 本地工具数据
