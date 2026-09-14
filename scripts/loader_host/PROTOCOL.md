# 电子负载上位机通信协议

本文件是**二进制帧规范**，作为固件侧实现的依据。上位机侧（`scripts/loader_host`）
已按本规范完整实现并有单元测试覆盖，固件实现后即可直接启用，上位机无需改动。

---

## 1. 为什么需要一个二进制协议

固件目前只有 Letter Shell 的 ASCII 命令控制台。它对人工调试很合适，但作为机器接口有几个问题：

| 问题 | ASCII | 二进制帧 |
|---|---|---|
| 解析成本 | 每行 printf 格式化，主机要写正则 | 定长字段，直接 `memcpy` |
| 带宽 | `lstatus` 九行约 400 B | 状态帧 64 B + 7 B 开销 |
| 数值精度 | `%.3f` 文本，有舍入 | IEEE-754 原始 4 字节 |
| 同步性 | 回显/提示符会插进输出 | magic + CRC 可重同步 |
| 主动上报 | 不支持 | TELEMETRY 可周期推送 |

关键约束：**二进制不能取代 shell**。串口是唯一的人机通道，如果切到二进制后 shell 死掉，
现场就没有救急手段了。因此协议设计为**与 shell 共存**（见 §5）。

---

## 2. 帧格式

```
偏移  长度  字段        说明
0     1     MAGIC0      0xA5
1     1     MAGIC1      0x5A
2     1     SEQ         主机分配，设备在响应中原样回带
3     1     CMD         操作码（响应时置高位 0x80）
4     1     LEN         载荷长度 0..192
5     N     PAYLOAD     载荷（N = LEN）
5+N   1     CRC_LO      CRC-16/CCITT-FALSE 低字节
6+N   1     CRC_HI      高字节
```

- **CRC 覆盖范围**：`SEQ | CMD | LEN | PAYLOAD`，即偏移 2 到 4+N（含），**不含** magic。
- **字节序**：CRC 与所有多字节数值字段均为小端。
- **最小帧长**：7 字节（LEN=0）。最大帧长：199 字节（LEN=192）。

### CRC-16/CCITT-FALSE

| 参数 | 值 |
|---|---|
| 多项式 | `0x1021` |
| 初始值 | `0xFFFF` |
| 输入反射 | 否 |
| 输出反射 | 否 |
| 结果异或 | `0x0000` |

**校验值**：`CRC("123456789") == 0x29B1`。固件实现时务必用这个向量自测。

参考实现（C，逐位）：

```c
uint16_t crc16_ccitt_false(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
        }
    }
    return crc;
}
```

### 为什么选 0xA5 0x5A

Letter Shell 对部分字节有按键语义，必须避开：

| 字节 | shell 语义 |
|---|---|
| `0x00` | `SHELL_ASSERT(data, return)` 直接丢弃 |
| `0x1B` | ESC，方向键序列首字节 |
| `0x09` | Tab 补全 |
| `0x08` / `0x7F` | 退格 / 删除 |
| `0x0A` / `0x0D` | 回车（LF / CRLF） |

`0xA5` 与 `0x5A` 都落在 `0x80–0xFF` 惰性区，不触发任何按键绑定。
即使一个噪声字节恰好是 `0xA5`，shell 也只是回显它（因为 `shellInsertByte` 无条件回显），
不会破坏状态机——因为**帧状态下 magic 前的字节才交给 shell**（见 §5）。

---

## 3. 命令表

### 请求（主机 → 设备）

| CMD | 名称 | 载荷 | 说明 |
|---|---|---|---|
| `0x01` | `PING` | 无 | 探活 |
| `0x10` | `GET_STATUS` | 无 | 请求完整状态快照 |
| `0x11` | `SET_SETPOINT` | `u8 channel` + `f32 value` | 设定值 |
| `0x12` | `SET_MODE` | `u8 mode` | 0=CC 1=CV 2=CP 3=CR |
| `0x13` | `RUN` | 无 | 请求启动输出 |
| `0x14` | `STOP` | 无 | 请求停机 |
| `0x15` | `CLEAR_FAULT` | 无 | 清故障 → IDLE |
| `0x16` | `SET_FAN` | `u8 enabled` + `f32 percent` | 风扇使能与占空比 |
| `0x17` | `GET_OUT` | 无 | 请求执行器读回 |

`SET_SETPOINT` 的 `channel` 取值：`0` = 当前模式的设定值（等价于 CLI 的 `lset <x>`），
`'i'`/`'v'`/`'p'`/`'r'` = 指定通道。

### 响应（设备 → 主机）

- **成功**：`CMD = 请求操作码 | 0x80`，载荷按命令定义。
- **失败**：`CMD = 0xFF`，载荷为 `i8 exit_code`（即固件的 `exit_code_t`，见
  `src/lib/tools/common_def.h`）。例如 `EXIT_BUSY = -6`。
- **主动上报**：`CMD = 0x98`（TELEMETRY），设备周期性推送，载荷同 `GET_STATUS`。

| 请求 | 成功响应载荷 |
|---|---|
| `PING` | 无 |
| `GET_STATUS` / `GET_OUT` / `TELEMETRY` | 64 字节状态块（§4） |
| `SET_SETPOINT` / `SET_MODE` / `RUN` / `STOP` / `CLEAR_FAULT` / `SET_FAN` | 无（成功即回一个空载荷响应） |

`RUN` 在设备处于 ERROR 时必须返回 `0xFF` + `EXIT_BUSY (-6)`，与 ASCII 侧
`lrun: FAIL EXIT_BUSY (-6)` 行为一致。

---

## 4. 状态载荷（64 字节）

固定布局，逐字节写死以避免结构体对齐分歧。**不含隐式 padding**——
所有 `f32` 都位于 4 字节对齐偏移上，但仍要求固件按字节填充而非直接 `memcpy` 结构体。

```
偏移  类型   字段
0     u8     state        0=IDLE 1=RUNNING 2=PAUSED 3=ERROR
1     u8     error        0=NONE 1=OCP 2=OTP 3=UVP
2     u8     mode         0=CC 1=CV 2=CP 3=CR
3     u8     out_enabled  0/1
4     u8     fan_enabled  0/1
5     u8[3]  reserved     填 0
8     f32    current_setpoint       (A)
12    f32    voltage_setpoint       (V)
16    f32    power_setpoint         (W)
20    f32    resistance_setpoint    (Ω)
24    f32    current_measurement    (A)
28    f32    voltage_measurement    (V)
32    f32    power_measurement      (W)
36    f32    resistance_measurement (Ω)
40    f32    temperature_measurement(°C)
44    f32    out_current            (A)
48    f32    out_voltage            (V)
52    f32    out_norm               (0..1)
56    f32    fan_speed              (0..1)
60    f32    fan_target             (0..1)
```

合计 64 字节。

### 关于 `current_measurement`

当前固件 `LOADER_USE_SOFTWARE_CURRENT_PID = 0`，`loader_core.c` 里
`current_measurement = load_out_get_current()`，即**下发值**而非 ADC 采样值。
固件实现本协议时应保持这一语义（与 `lmeas` 一致），上位机界面已明确标注
「电流为下发值（硬件闭环），非 ADC 采样」。若将来启用软件电流环，此字段才变为真实采样。

---

## 5. 与 Letter Shell 共存（magic 分派）

**没有模式切换，没有握手，shell 始终在线。**

设备端 RX 处理逻辑（与上位机 `demux.py` 对称）：

```
状态 SHELL:
    收到 0xA5 → 进入 FRAME，暂存该字节（不交给 shell）
    其他字节 → 交给 shellHandler()
状态 FRAME 中:
    按 §2 收满一帧 → 校验 CRC
        CRC 正确 → 执行命令
        CRC 错误 / LEN 非法 → 丢弃整帧，回到 SHELL
    帧内字节间隔超过 50 ms → 作废半帧，回到 SHELL
```

要点：

1. **magic 前的字节交给 shell**，所以 ASCII 命令（`lstatus` 等）在二进制模式下依然可用，
   这是现场救急通道，也意味着上位机崩溃不需要复位设备。
2. **伪 magic 的代价极低**：帧外收到 `0xA5` 后如果下一个字节不是 `0x5A`，
   只需丢掉那个 `0xA5`；后续字节不是 magic 就正常还给它 shell。
3. **帧内超时是必需的**：主机发帧中途断开时，否则状态机会永远卡在帧内。
4. **TX 侧**：二进制响应只能用 DMA 直发通道，**绝不能经 `logPrintln` 路径**
   （那条路径带 ANSI 颜色码和 `\r\n`，会污染帧）。若设备在二进制会话中也可能收到
   ASCII 命令并打印日志，建议在发送帧前后短暂压掉 UART 日志对象（`log_obj.active = 0`）。

### 缓冲区约束

- RX StreamBuffer 仅 256 字节（`src/driver/uart/uart_async.c`）。StreamBuffer 容量不足时
  `xStreamBufferSendFromISR` 会**整块丢弃**并累加 `rx_dropped`，而该计数器目前没有任何读取点。
  若实现 TELEMETRY 周期推送，建议同时把 `rx_dropped` / `rx_errors` 暴露到状态载荷或 CLI。
- 无硬件流控（`UART_HWCONTROL_NONE`），批量传输需靠 CRC + 重传。

---

## 6. 交互示例

主机请求 `GET_STATUS`（SEQ=0x07）：

```
A5 5A 07 10 00 CRC_LO CRC_HI
```

设备成功响应（CMD = 0x10|0x80 = 0x90，载荷 64 字节）：

```
A5 5A 07 90 40 <64 bytes status> CRC_LO CRC_HI
```

设备在 ERROR 状态下收到 `RUN`（SEQ=0x08），返回失败：

```
A5 5A 08 FF 01 FA CRC_LO CRC_HI
```

`0xFF` = 错误响应；`01` = LEN=1；`FA` = -6 的单字节补码，即 `EXIT_BUSY`。

---

## 7. 固件侧实现清单

- [ ] `crc16_ccitt_false()`，用 `CRC("123456789") == 0x29B1` 自测
- [ ] RX 字节状态机（§5），帧内 50 ms 超时
- [ ] 命令分发到现有 `loader_core_request_*` / `loader_runtime_set_*` API，
      **不新增写执行器的路径**（保持 `loader_core` 独占 DAC）
- [ ] 64 字节状态打包（§4），字段取自 `loader_runtime_get()` + `load_out_*` + `fan_*`
- [ ] 二进制响应走独立发送函数，不经 `logPrintln`
- [ ] 可选：TELEMETRY 周期推送，以及把 `rx_dropped` / `rx_errors` 暴露出来

上位机侧无需改动即可对接；`tests/test_binary_proto.py` 与 `tests/test_demux.py`
已经固定了帧布局、CRC 向量和重同步行为。
