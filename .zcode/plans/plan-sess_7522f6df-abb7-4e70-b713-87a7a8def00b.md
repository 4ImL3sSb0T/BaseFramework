# 电子负载上位机（scripts/loader_host）

## 目标与约束

在 `scripts/loader_host/` 新建一个 **PySide6 + pyqtgraph** 的深色仪器风格上位机，用 `uv` 管理环境，通过 USART1（115200 8N1）与固件通信：

- **通信策略（已定）**：ASCII 命令与二进制帧**共存**，靠 magic 分派。ASCII 是今天就能跑、能验证的主链路；二进制帧编解码按约定实现并单测，作为可选链路，固件加好后直接切换，上位机不重写。
- **用途（已定）**：实时调试台与长时程记录兼顾。
- 固件侧**当前不存在任何二进制协议**，所以二进制部分在上位机侧可完整实现+单测，端到端需固件配合。
- 本机目前只有蓝牙虚拟串口（COM6/7），无 USB‑TTL，因此**内置设备模拟器**是必需项，否则无法开发与验证。

## 目录结构（照 `scripts/map_analyzer` 的工程约定）

```
scripts/loader_host/
├── pyproject.toml          # deps: pyserial, PySide6, pyqtgraph；dev: pytest, ruff
├── .python-version         # 3.11（与 map_analyzer 一致，本机已缓存 3.11.14）
├── .gitignore              # 抄 map_analyzer，但保留 uv.lock 入版本控制（有第三方依赖，锁文件更有价值）
├── README.md               # 用法、如何接真机 / 起模拟器
├── PROTOCOL.md             # 二进制帧规范 —— 固件侧实现依据
├── main.py                 # 入口 shim（对齐 analyze_map 的 main.py）
├── loader_host/
│   ├── __init__.py
│   ├── __main__.py         # python -m loader_host
│   ├── config.py           # 限值/枚举/默认值，镜像 firmware 常量
│   ├── model.py            # LoaderSnapshot 等 dataclass
│   ├── ascii_proto.py      # 命令构造 + 容错行解析
│   ├── binary_proto.py     # A5 5A 帧编解码 + CRC16
│   ├── demux.py            # 帧/ASCII 字节级分派状态机（含帧内超时）
│   ├── transport.py        # Transport 抽象 + SerialTransport + SimulatorTransport
│   ├── client.py           # 轮询调度 + 命令队列 + 快照信号
│   ├── recorder.py         # CSV 记录 / 导出 / 回放
│   ├── theme.py            # 深色仪器主题（自维护 QSS + pyqtgraph 配色，不引额外主题库）
│   ├── main_window.py      # 主窗口与布局
│   └── panels.py           # 各面板控件
└── tests/
    ├── test_ascii_proto.py
    ├── test_binary_proto.py
    └── test_demux.py
```

进入方式：`uv run loader-host`（`[project.scripts]`）或 `uv run main.py`。

## 关键技术设计

### 1. 线程模型（GUI 不能卡）
Qt 主线程只做界面。串口 I/O 放在 `QThread` 承载的 worker 里，用 `queue.Queue` 收上位机下发的命令，用信号把数据交回主线程。

- worker 循环：短超时（20–50 ms）读串口 → 喂 demux → 发信号；每轮先排空命令队列，保证顺序且不阻塞 GUI。
- 信号：`snapshot_updated(LoaderSnapshot)`、`line_received(str)`、`raw_bytes(bytes)`、`connection_changed(bool)`、`error_occurred(str)`。
- 线程安全靠 Qt 队列连接，不共享可变状态。

### 2. 轮询调度（尊重 115200 的带宽）
- 双层轮询：`lmeas`（单行，约 60 B）默认 **10 Hz** 驱动曲线；`lstatus`（9 行，约 400 B）默认 **1 Hz** 刷新 state/mode/set/limits/out/fan。两者独立可调。
- 请求-响应式，同一时刻只允许一个在途请求，避免回复交错。

### 3. ASCII 解析必须容错（不回显污染）
设备会回显输入字符、打印 `letter:/$ ` 提示符、启动 banner、以及 `\033[..m` 颜色码。因此：

- 先剥离 ANSI，再按行匹配已知格式（regex），**匹配不到的行静默丢弃**，不做严格协议同步。
- 覆盖 `loader_cli.c` 中全部输出格式（已逐一核对，含 `%.3fohm`、字段间双空格等细节）：`lstatus` 的 state/error/mode/set/active/meas/out/fan/limits、`lmeas` 单行、`lrun|lstop|lclr` 的 `: OK` / `: FAIL <name> (<n>)` 与续行、`lmode`、`lset`（含 `bad number` / 三种用法错误）、`lout`、`lfan`。

### 4. 二进制帧规范（写入 `PROTOCOL.md`，固件据此实现）
```
0  1  MAGIC0=0xA5
1  1  MAGIC1=0x5A
2  1  SEQ          主机分配，响应原样回带
3  1  CMD
4  1  LEN          0..192
5  N  PAYLOAD
5+N 1 CRC_LO       CRC-16/CCITT-FALSE(poly 0x1021, init 0xFFFF, no reflect/xorout)
6+N 1 CRC_HI       覆盖 SEQ|CMD|LEN|PAYLOAD，小端
```
- 命令：`0x01 PING`、`0x10 GET_STATUS`、`0x11 SET_SETPOINT`、`0x12 SET_MODE`、`0x13 RUN`、`0x14 STOP`、`0x15 CLEAR_FAULT`、`0x16 SET_FAN`、`0x17 GET_OUT`；响应置高位（`0x80|cmd`），错误用 `0xFF` + `exit_code`。另设 `0x98` 周期性 TELEMETRY（设备主动上报，二进制真正价值所在）。
- **magic 选 `0xA5 0x5A`**：两个字节都不落在 Letter Shell 的按键语义里（`0x00/0x1B/0x09/0x08/0x7F/0x0A/0x0D`），落在 `0x80–0xFF` 惰性区，误判与残留代价最低。
- 状态快照 payload 的字段偏移在 `PROTOCOL.md` 里逐字节写死（state/error/mode + 各 f32 设定与测量 + out/fan），避免结构体对齐分歧。

### 5. demux（对称于固件侧设计）
字节级状态机：`SHELL → MAGIC1 → CMD → LEN → PAYLOAD → CRC`。非法长度整帧丢弃；帧内字节间隔超时（默认 50 ms）则作废半帧回到 SHELL。非帧字节交给 ASCII 行组装器。

### 6. 模拟器（无硬件也能开发/验证）
`SimulatorTransport` 在同一 `Transport` 接口下提供假设备：

- 复现真实 shell 行为：**回显输入 + 打印提示符**，让解析器的容错能力被真实地测到。
- 解析上位机 ASCII 命令并更新内部状态（`lmode`/`lset`/`lrun`/`lstop`/`lclr`/`lfan`），响应文字与固件逐字符一致。
- 简单 DUT 模型（CC 下电流趋向设定、电压由假负载决定、P=V·I、R=V/I、温度缓升），让曲线看起来真实。
- **同时实现二进制帧响应**，使帧编解码能走完整的 transport/worker 链路做端到端验证。

### 7. 界面（深色仪器风格，自维护 QSS）
单窗口，左控制 / 中曲线 / 右状态，底部可折叠终端：

- **连接栏**：端口下拉（`serial.tools.list_ports` 带友好名）、波特率（默认 115200）、连接/断开、模拟器开关；用 `QSettings` 记住上次端口与布局。
- **读数区**：V/I/P/R/T 五块大字号等宽数字卡片。
- **曲线区**：pyqtgraph 实时曲线，多通道可勾选，滚动窗口（默认 60 s），配色与主题同源。
- **控制区**：CC/CV/CP/CR 模式切换；设定值输入用 `QDoubleSpinBox` 按固件限值设范围并钳位（I≤5 A、V≤30 V、P≤50 W、R≤1000 Ω）；Run/Stop/Clear Fault；风扇 on/off/百分比。
- **状态区**：state/error 徽标、故障时醒目红色横幅、OCP=5 A / OTP=50 °C 阈值就近告警（测量逼近阈值时变色）；`out` 执行器读回。
- **终端面板**：原始 shell 终端（magic 分派方案下 shell 始终可用，这是逃生通道，也可用来手工验证）。
- **二进制面板**：开关二进制链路，显示 TX/RX 帧数、CRC 错误、重新同步次数；默认关闭并标注「需固件支持」。

### 8. 记录（兼顾长时程）
按选定时长把时间戳 + 全部快照字段写 CSV，支持导出与**回放**（读回 CSV 重新绘图），便于复盘。

## 必须编码进实现的正确性细节

1. **`current_measurement` 是下发值不是 ADC 采样值**（`LOADER_USE_SOFTWARE_CURRENT_PID=0`，`loader_core.c` 里取的是 `load_out_get_current()`）。界面上要明确标注「电流为下发值（硬件闭环），非 ADC 采样」，不能让用户误读。
2. **设定值是静默钳位**，不会返回失败。发完 `lset` 后必须回读 `lset: OK ...` 的实际值并刷新 UI。
3. **`lrun` 仅在 `state==ERROR` 时被拒**（返回 `EXIT_BUSY=-6`）。UI 在这种情况提示先 Clear Fault，而不是当成一般错误。
4. `PAUSED` 与 `UVP` 是**未使用的保留枚举**，不为其做界面分支；`temperature_setpoint` 无任何代码路径写入，不展示。

## 验证方式

1. `uv run pytest` —— ascii 解析（喂入从 `loader_cli.c` 抄录的真实行）、binary 编解码（往返、CRC 已知向量、注入错误后能重同步）、demux（magic 跨块切分、伪 magic、超时回退）。
2. `uv run loader-host --sim` —— 无硬件跑起完整界面，逐面板确认：曲线在动、模式/设定/开关生效、故障横幅可用、终端可交互、二进制面板计数增长。
3. 之后接真机验证：`uv run loader-host -p <COMx>`，用终端面板确认回显与提示符被正确忽略、读数与 `lstatus` 文本一致。
4. 我会实际运行并截图给你看「美观」是否达标，按你的反馈调主题与布局。

## 备注

- 端口/波特率不写死，默认值与固件一致（115200 8N1 无流控）。
- 开发顺序：脚手架 → config/model → ascii 解析+测试 → binary 编解码+PROTOCOL.md+测试 → demux+测试 → transport/模拟器 → client 轮询 → recorder → UI → 联调与截图。