---
name: serial-monitor
description: |
  串口调试工具，用于读取 GD32 开发板的串口输出。
  当用户提到串口、serial、调试输出、查看日志、monitor 等关键词时触发此 skill。
  也适用于需要监听 COM 口数据或调试嵌入式设备的场景。
---

# Serial Monitor

GD32F470 串口调试监听工具，默认 115200bps。

## 工作流程

### 1. 列出可用串口

```powershell
Get-PnpDevice -Class Ports -Status OK | Select-Object FriendlyName, Status
```

### 2. 读取串口数据（单次）

```powershell
$port = New-Object System.IO.Ports.SerialPort('COM18', 115200, 'None', 8, 'One')
$port.Open()
Start-Sleep -Seconds 3
$data = $port.ReadExisting()
Write-Host $data
$port.Close()
```

### 3. 持续监听（使用 Monitor 工具）

对于持续监听场景，使用 Monitor 工具：

```powershell
$port = New-Object System.IO.Ports.SerialPort('COM18', 115200, 'None', 8, 'One')
$port.Open()
while ($true) {
    $data = $port.ReadExisting()
    if ($data) { Write-Output $data }
    Start-Sleep -Milliseconds 100
}
```

## 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| COM 口 | COM18 | 根据实际设备调整 |
| 波特率 | 115200 | 与固件配置一致 |
| 数据位 | 8 | |
| 校验位 | None | |
| 停止位 | 1 | |

## 注意事项

- 需要管理员权限才能访问某些串口
- 确保串口没有被其他程序占用（如串口调试助手、SSCOM）
- 如果没有收到数据，检查：
  - 串口号是否正确（设备管理器查看）
  - 波特率是否与固件匹配
  - 设备是否已上电并发送数据
  - USB 线是否连接正常
