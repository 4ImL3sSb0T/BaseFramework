---
name: keil-build
description: |
  Keil MDK 编译和下载工具，用于 STM32H750 项目的构建流程。
  当用户提到编译、构建、烧录、下载、flash、keil 等关键词时触发此 skill。
  也适用于代码修改后需要验证编译的场景。
---

# Keil Build & Flash

STM32H750 项目的 Keil MDK 编译和下载流程。

## ⚠️ 必须使用 PowerShell

**所有命令必须通过 `PowerShell` 工具执行，禁止使用 `Bash` 工具。**

原因：Keil 编译命令使用 PowerShell 语法（`Start-Process`、`Get-Content`、`Write-Host` 等），Bash 无法识别这些命令。

## 工程配置

| 参数 | 值 |
|------|-----|
| 工程文件 | `BaseFramework.uvprojx` |
| 编译输出 | `build_output.txt` |
| Keil 路径 | `C:\Users\yuang\AppData\Local\Keil_v5\UV4\UV4.exe` |
| 项目目录 | `D:\Project\H750\BaseFramework\MDK-ARM` |

## 工作流程

### 1. 编译项目

使用 `PowerShell` 工具执行：

```powershell
$proc = Start-Process -FilePath "C:\Users\yuang\AppData\Local\Keil_v5\UV4\UV4.exe" `
  -ArgumentList "-b", "D:\Project\H750\BaseFramework\MDK-ARM\BaseFramework.uvprojx", `
                "-o", "D:\Project\H750\BaseFramework\MDK-ARM\build_output.txt" `
  -PassThru -Wait -NoNewWindow
Write-Host "Exit code: $($proc.ExitCode)"
```

### 2. 检查编译结果

使用 `PowerShell` 工具执行：

```powershell
Get-Content D:\Project\H750\BaseFramework\MDK-ARM\build_output.txt | Select-Object -Last 5
```

**判断标准：**
- `0 Error(s)` → 编译成功
- `X Error(s)` → 编译失败，必须修复后重新编译
- exit code 1 → 可能有 warning，需检查输出确认

### 3. 下载（需用户明确指示）

仅当用户说"烧录"、"下载"、"flash"等关键词时执行。

使用 `PowerShell` 工具执行：

```powershell
$proc = Start-Process -FilePath "C:\Users\yuang\AppData\Local\Keil_v5\UV4\UV4.exe" `
  -ArgumentList "-f", "D:\Project\H750\BaseFramework\MDK-ARM\BaseFramework.uvprojx" `
  -PassThru -Wait -NoNewWindow
Write-Host "Exit code: $($proc.ExitCode)"
```

**判断标准：**
- `Erase Done. Programming Done. Verify OK.` → 下载成功
- 其他 → 下载失败，检查连接和供电

## 完整流程

1. 修改代码（Edit/Write）
2. 调用此 skill 编译（**必须用 PowerShell 工具**）
3. 检查输出：`0 Error(s)` → 通过
4. 有 Error → 修复 → 回到 2
5. 用户说"烧录" → 执行下载
6. 报告结果

## 注意事项

- **必须使用 `PowerShell` 工具，禁止使用 `Bash` 工具**
- 编译自动执行，无需用户确认
- 下载必须用户明确指示（"烧录"、"下载"、"flash"）
- 有 Error 必须修复后才能下载
- 编译输出 `build_output.txt` 每次覆盖

## CubeMX 兼容性

本项目是 CubeMX 生成的项目，修改代码时需注意：

- **`Core/Inc/**` 和 `Core/Src/**`**：用户代码区，CubeMX 不会覆盖
- **`FreeRTOSConfig.h`**：修改必须放在 `USER CODE BEGIN/END` 保护区内
- **工程文件 `.uvprojx`**：修改后**会**被 CubeMX 覆盖。重新生成工程后需手动恢复：
  1. Include 路径添加 `../Core/Inc/SEGGER`
  2. 添加工程组 `Application/User/SEGGER` 及其源文件
  3. 修改 MDK-ARM Target 为 BaseFramework 对应的配置

### SEGGER SystemView 文件位置

- 头文件：`Core/Inc/SEGGER/`（9 个 .h 文件）
- 源文件：`Core/Src/SEGGER/`（5 个 .c + 1 个 .S 文件）
