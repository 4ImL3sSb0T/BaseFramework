# 电子负载软件设计思路

> 目标：单通道、实验板级（&lt;50W 量级）、CC/CV/CR/CP 全模式。  
> 人机：TFT + MujicaUI + 按键；调试：串口 CLI/日志。  
> 现状：功率级硬件未定 → **先定接口与分层，用 mock 跑通控制与状态机**。

本文只讲**怎么拆、怎么写、依赖怎么走**，不展开具体寄存器与引脚。

---

## 1. 总体分层

手写代码全部在 `src/`。`Core/` 是 CubeMX 生成代码。

```
┌─────────────────────────────────────────────────────────┐
│  app                 这台负载：状态机、模式、任务、UI/CLI │
├─────────────────────────────────────────────────────────┤
│  service             领域能力：sense / load_out / fan    │
├─────────────────────────────────────────────────────────┤
│  driver              外设驱动：ADC / DAC / TIM / UART    │
├─────────────────────────────────────────────────────────┤
│  bsp                 板级平台：gpio / rtt / sys / board  │
├─────────────────────────────────────────────────────────┤
│  Core/HAL            Cube 生成：时钟、外设、HAL、RTOS     │
└─────────────────────────────────────────────────────────┘
         依赖方向：app → service → driver → bsp → Core/HAL
         lib/ 是算法与第三方，不当一层
```

| 层 | 一句话职责 | 电子负载里放什么 |
|----|------------|------------------|
| **app** | 这台负载 | 模式、状态机、控制编排、UI、CLI |
| **service** | 领域能力 | sense / load_out / fan / shell / storage |
| **driver** | 外设驱动 | ADC / DAC / TIM / UART / TFT / flash |
| **bsp** | 板级平台 | board / gpio / rtt / sys |
| **lib** | 库 | PID、shell body、lfs body、GFX（不当层） |

判断标准：

- 代码里出现 **「设定电流 / CC 模式 / 故障恢复」** → **app**
- 代码里出现 **「伏特、安培」** → **service**
- 代码里出现 **「这个引脚、ADC 码值」** → **driver / bsp**
- 代码里出现 `MX_*` / Cube 外设初始化 → **Core/**，不要手写新文件

---

## 2. app 文件与模块（精简）

`src/app` 平铺，**5 个模块**：

```
src/app/
├── loader_config.h      周期、限值、默认 PID（仅头文件）
├── loader_runtime.h     共享状态（唯一真相源）
├── loader_runtime.c
├── loader_core.h        fsm + mode + control 合一
├── loader_core.c
├── loader_ui.c          显示/按键（声明在 loader_task.h）
├── loader_cli.c         串口命令（声明在 loader_task.h）
├── loader_task.h
└── loader_task.c        FreeRTOS 任务与周期调度
```

| 模块 | 文件 | 职责 |
|------|------|------|
| **config** | `loader_config.h` | 编译期常量：周期、满量程、默认 PID/限值 |
| **runtime** | `loader_runtime.*` | 设定 / 测量 / 状态，全员读写边界 |
| **core** | `loader_core.*` | 状态机 + 四模式目标 + PID 环；**唯一写执行器** |
| **ui / cli** | `loader_ui.c` / `loader_cli.c` | 只改设定、只读 runtime；不写 DAC |
| **task** | `loader_task.*` | 建任务、定周期；不承载业务公式 |

### 2.1 runtime：共享状态

三类数据：

1. **设定（人写）**：模式、设定值、输出开/关  
2. **测量（环写）**：电压、电流、功率、温度（来自 service）  
3. **结果（环写）**：控制输出、故障码、当前状态  

约定：

- **写执行器**：只允许 core（且状态允许时）  
- **写设定**：UI / CLI  
- **读测量**：谁都可以；写测量只在 core 周期内从 sense 更新  

### 2.2 core：状态机 + 模式 + 控制环

逻辑上仍是三块，**实现放在同一个 `loader_core.c`**，避免文件碎：

```
fsm     IDLE / RUN / FAULT，管「能不能输出」
mode    CC/CV/CR/CP → 本周期 I_target
control 测 → 护 → 目标 → PID → 写输出
```

状态：

```
        上电
          │
          ▼
       IDLE ──── ON 且无故障 ────► RUN
          ▲                          │
          │        保护 / OFF         │
          │◄─────────────────────────┤
          │                          │
          └── 清除故障 ◄── FAULT ◄───┘
```

| 状态 | control 是否允许驱动功率级 |
|------|----------------------------|
| IDLE | 否 |
| RUN  | 是 |
| FAULT| 否，需明确恢复 |

模式策略：

> **内环统一做电流**；CV/CR/CP 只算 `I_target`。

| 模式 | 目标怎么来 |
|------|------------|
| **CC** | `I_target = I_set` |
| **CV** | 电压外环（或限流）生成 `I_target` |
| **CR** | `I_target = V_meas / R_set` |
| **CP** | `I_target = P_set / max(V_meas, ε)` |

以后 core 变胖再拆 `loader_fsm` / `loader_mode`，现在不必。

### 2.3 ui / cli

- 显示：读 runtime  
- 输入：改 mode、setpoint、output_on（经 core 的 request API 或直接写设定字段）  
- **禁止**：直接 `HAL_DAC` / 写使能 GPIO  
- CLI 与 UI 同一套设定接口  

ui/cli **不单独建 .h**，入口声明放在 `loader_task.h`。

### 2.4 task：胶水与节奏

| 谁 | 周期量级 | 职责 |
|----|----------|------|
| 控制任务（高） | 1–10 ms | 调 `loader_core_step()` |
| UI 任务（低） | 20–50 ms | `loader_ui_poll()` |
| CLI | 事件/空闲 | shell 命令（`loader_cli_init` 注册） |

硬件未定时 sense/输出可 mock，**任务切分和周期先定死**。

### 2.5 依赖（只允许实线方向）

```
   ui ──┐
   cli ─┼──► runtime ◄── core
        │       ▲
        └───────┘（设定 / 开关请求可走 core API）

   task 调用：core / ui；不承载业务公式
                │
                ▼
            service（sense / protect）
                │
                ▼
              bsp
```

---

## 3. 控制部分怎么写

### 3.1 边界

- **PID 本体** → `common/pid`  
- **环编排** → `loader_core`  
- **测量** → service，core 不解析 ADC raw  
- **DAC** → bsp；core 只输出 `out_norm ∈ [0, 1]`（或等价抽象）

```
core 输出：float out_norm  ∈ [0, 1]
     ↓
service/load_out：映射到 DAC / PWM，并处理使能时序
```

### 3.2 单周期伪代码（`loader_core_step`）

```text
meas = sense_get()
runtime 更新测量
fault = protect_check(meas, limits)
if fault:
    → FAULT；output_disable()
    return

I_target = mode_compute(mode, setpoint, meas)
u = pid_step(pid, I_target, meas.i)
u = clamp(u, 0, 1)

if state == RUN && output_on:
    load_out_set(u)
else:
    load_out_set(0) / disable
```

### 3.3 实现顺序

1. **CC + 保护 + 开关机**  
2. **CR / CP**（代数算 `I_target`）  
3. **CV**（注意稳定性，优先限流/限斜率）  

### 3.4 保护

- **protect**（service）：只判定  
- **core（fsm）**：关断与锁定  
- 保护逻辑不要在 UI/CLI 各写一遍  

### 3.5 标定与滤波

| 内容 | 层 |
|------|-----|
| 滑动平均 / 低通 | common 或 service 调 common |
| raw → V/A | service |
| 控制用 / 显示用带宽 | 需要时在 sense 出两路，不在 core 再滤一套 |

### 3.6 Mock

- `sense_get()`：假 V/I（可随假输出变化）  
- `load_out_set(u)`：只记录 u，可选 `I = f(u)`  
- 板子来了只换 service/driver  

---

## 4. 文件一览

| 文件 | 角色 |
|------|------|
| `loader_config.h` | 默认限值、PID、周期、满量程 |
| `loader_runtime.h/.c` | `loader_runtime_t` 与访问 |
| `loader_core.h/.c` | fsm + mode + control + PID 实例 |
| `loader_ui.c` | 显示与按键 |
| `loader_cli.c` | 串口命令 |
| `loader_task.h/.c` | 产品入口：init、建任务；ui/cli 声明 |

---

## 5. 设计原则

1. **一个真相源**：设定与测量以 runtime 为准。  
2. **一个写执行器的人**：core（受状态门控）。  
3. **一个故障决策点**：core 内 fsm + protect，禁止多处关断策略不一致。  
4. **模式换目标，不换整套架构**：统一电流内环。  
5. **先接口后硬件**：bsp 可 mock。  
6. **实时与人机分离**：控制任务短而固定；UI/日志可慢。  

---

## 6. 落地节奏

```
① runtime + core 内 IDLE/RUN/FAULT
② mock sense / load_out
③ CC + 输出开关
④ OCP/OVP → FAULT
⑤ UI
⑥ CLI
⑦ CR、CP
⑧ CV
⑨ 真硬件 + 标定
```

---

## 7. 一句话总结

- **工程分层**：bsp 硬件 → service 物理量/保护 → common 算法 → app 产品逻辑。  
- **app 精简**：runtime 中心；**core** 管状态+目标+闭环；ui/cli 只改设定；task 管节奏。  
- **控制**：统一内环 + 模式算目标；周期内「测→护→算→写」；mock 先把环跑稳。

硬件原理图确定后，补通道、执行器、限值表和 `doc/hardware.md` 即可——**不必推翻上述分层**。
