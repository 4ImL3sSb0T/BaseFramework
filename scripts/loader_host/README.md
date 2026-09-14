# 电子负载上位机 (loader-host)

STM32H750 电子负载固件的 PC 端上位机。通过 **USART1（115200 8N1）** 与固件通信，
实时显示 V/I/P/R/T 曲线，控制模式与设定值，支持 CSV 记录与回放。

界面为深色仪器风格（PySide6 + pyqtgraph），依赖 `uv` 管理。

---

## 快速开始

```bash
cd scripts/loader_host

# 1) 装依赖（首次会下载 PySide6，约 250 MB）
uv sync

# 2) 无硬件也能跑：内置设备模拟器
uv run loader-host --sim

# 3) 接真机
uv run loader-host --list-ports        # 先看看有哪些串口
uv run loader-host -p COM7             # 默认 115200
uv run loader-host -p COM7 -b 115200
```

也可以不开 GUI 只列端口：

```bash
uv run loader-host --list-ports
```

---

## 界面说明

| 区域 | 内容 |
|---|---|
| 顶部工具栏 | 端口 / 波特率选择、连接、刷新、链路状态 |
| 左侧 · 工作模式 | CC / CV / CP / CR 切换 |
| 左侧 · 设定值 | 按当前模式的量程输入并下发（固件会静默钳位） |
| 左侧 · 输出控制 | 启动 / 停止 / 清除故障，含状态横幅 |
| 左侧 · 风扇 | 开 / 关 / 百分比 |
| 左侧 · 记录 | CSV 记录、导出、回放 |
| 中部 · 读数 | V / I / P / R / T 五个大字号卡片，接近限值自动变琥珀/红 |
| 中部 · 曲线 | 五通道实时曲线，可勾选，滚动窗口 60 s，鼠标悬停读数 |
| 右侧 · 状态 | state / error / mode / 当前设定 |
| 右侧 · 执行器读回 | out 的 en / Iref / Vref / norm |
| 右侧 · 限值 | OCP / OTP / Imax / Vmax / Pmax / Rmax |
| 底部 · 串口终端 | 原始 shell 控制台，可手工敲 `lstatus` 等命令 |
| 底部 · 二进制协议 | 二进制链路开关与统计（见下） |

---

## 通信方式

上位机**同时支持**两种链路，靠 `0xA5 0x5A` magic 在字节层面自动分派：

1. **ASCII（今天就能用）** — 现有固件的 Letter Shell 命令（`lmeas` / `lstatus` / `lrun` / `lstop` /
   `lclr` / `lmode` / `lset` / `lout` / `lfan`）。默认以 10 Hz 轮询 `lmeas` 驱动曲线、1 Hz 轮询
   `lstatus` 刷新慢变量。
2. **二进制帧（待固件支持）** — 定长字段 + CRC16，带宽更省、可主动上报。规范见
   [`PROTOCOL.md`](PROTOCOL.md)，上位机侧已完整实现并有单元测试；固件实现后勾选
   「启用二进制链路轮询」即可切换，上位机无需改动。

### 为什么 shell 一直在线

方案是 **magic 分派**，不是「关掉 shell」。读到 `0xA5 0x5A` 就进帧解析器，其余字节交给
`shellHandler`。这带来两个实际好处：

- 上位机崩溃或跑飞时，敲个回车就能回到 shell，**不需要复位设备**；
- 调试时可以交替发 ASCII 命令和二进制帧。

magic 选 `0xA5 0x5A` 是因为这两个字节落在 Letter Shell 的惰性区
（避开 `0x00`/`0x1B`/`0x09`/`0x08`/`0x7F`/`0x0A`/`0x0D`），不会触发按键绑定或擦除。

---

## 关于固件的两个语义（界面已如实标注）

1. **电流是下发值，不是 ADC 采样值。** 当前固件 `LOADER_USE_SOFTWARE_CURRENT_PID = 0`，
   `loader_core.c` 里 `current_measurement = load_out_get_current()`。界面在电流读数下标注
   「下发值」，避免误读为实测电流。启用软件电流环后才会变成真实采样。
2. **设定值是静默钳位，不会返回失败。** 固件把越界值裁到量程内并回显实际值，
   所以上位机在 `lset` 之后以回显值为准刷新界面，而不是假设用户输入即为生效值。

另外 `lrun` **只在设备处于 ERROR 时被拒绝**（返回 `EXIT_BUSY`），此时界面会提示先清除故障。

---

## CSV 记录与回放

- 「开始记录 CSV」写入 `logs/loader_<时间戳>.csv`，每行一个快照，**逐行 flush**，
  长时程采集不会因异常退出丢数据。
- 「导出…」把当前快照写出一行，适合留档。
- 「回放…」载入历史 CSV 重新绘图，便于复盘（缺失单元格保持为空隙而非 0）。

列定义见 `loader_host/recorder.py` 的 `COLUMNS`。

---

## 开发

```bash
uv run pytest          # 87 个用例：ASCII 解析 / 二进制编解码 / demux / 模拟器
uv run ruff check .    # lint
uv run ruff format .   # 格式化
```

### 模块结构

```
loader_host/
├── config.py        固件常量与枚举的镜像（限值、默认值、exit_code）
├── model.py         LoaderSnapshot / CommandResult
├── ascii_proto.py   shell 命令构造 + 容错行解析
├── binary_proto.py  A5 5A 帧编解码 + CRC16/CCITT-FALSE
├── demux.py         字节级帧/ASCII 分派状态机（含帧内超时）
├── transport.py     SerialTransport + SimulatorTransport
├── client.py        QThread worker：轮询调度 + 命令队列 + 信号
├── recorder.py      CSV 记录 / 导出 / 回放
├── theme.py         深色主题（QSS + pyqtgraph 配色）
├── panels.py        读数卡片 / 状态横幅 / 曲线 / 终端
├── main_window.py   主窗口与布局
└── app.py           入口与参数解析
```

### 解析为什么必须容错

USART1 是**人机控制台**而非帧协议：shell 会回显每个输入字节、提示符 `letter:/$ ` 不带换行
（会粘在下一条回复前面）、启动还有带 ANSI 颜色的 banner。所以解析器先剥离 ANSI 与提示符，
再按已知格式做**两端锚定**匹配，匹配不到就丢弃——不做严格协议同步。
`SimulatorTransport` 特意复刻了这些「恼人」行为（含回显和提示符），
让容错能力在端到端链路上被真实测到，而不只是单测里。

---

## 已知限制

- 固件尚未实现二进制协议；该链路默认关闭，界面已标注。
- 无硬件流控，批量传输需靠 CRC 与重传。
- 固件 RX StreamBuffer 仅 256 字节，且 `rx_dropped` 计数器未对外暴露，
  串口丢字节时上位机侧无法察觉——建议固件侧把该计数暴露出来（见 `PROTOCOL.md` §5）。
