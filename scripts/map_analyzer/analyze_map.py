#!/usr/bin/env python3
"""ARM armlink MAP analyzer — Flash / RAM usage for Keil MDK projects.

Parses Arm Compiler 6 armlink .map files and reports:
  - Load / execution region fill rates
  - Per-object and per-category Flash (ROM) ranking
  - Largest sections in Flash
  - Grand totals

Usage:
  uv run analyze_map.py [map_path]
  uv run analyze_map.py --top 30 --category
  uv run analyze_map.py --json
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path


# ---------------------------------------------------------------------------
# Data models
# ---------------------------------------------------------------------------


@dataclass
class ComponentSize:
    name: str
    code: int = 0
    code_inc_data: int = 0  # data embedded in code (subset of code)
    ro_data: int = 0
    rw_data: int = 0
    zi_data: int = 0
    debug: int = 0
    kind: str = "object"  # object | library | library_member

    @property
    def flash(self) -> int:
        """Primary Flash footprint: Code + RO Data.

        Per-object RW Data from armlink often includes large UNINIT /
        COMPRESSED regions (e.g. FreeRTOS heap in DTCM) that barely
        occupy Flash. Image-level Total ROM is authoritative for the
        full Flash image; object ranking uses Code+RO for fairness.
        """
        return self.code + self.ro_data

    @property
    def rom_reported(self) -> int:
        """Armlink-style Code+RO+RW (can over-count UNINIT RW)."""
        return self.code + self.ro_data + self.rw_data

    @property
    def ram(self) -> int:
        """Bytes that occupy RAM at runtime: RW + ZI."""
        return self.rw_data + self.zi_data

    @property
    def ro(self) -> int:
        return self.code + self.ro_data


@dataclass
class Region:
    name: str
    kind: str  # load | exec
    base: int = 0
    size: int = 0
    max_size: int = 0
    load_base: int | None = None
    compressed: int | None = None
    attrs: str = ""

    @property
    def free(self) -> int:
        return max(0, self.max_size - self.size) if self.max_size else 0

    @property
    def usage_pct(self) -> float:
        if not self.max_size:
            return 0.0
        return 100.0 * self.size / self.max_size


@dataclass
class SectionEntry:
    exec_addr: int
    load_addr: int | None
    size: int
    typ: str
    attr: str
    name: str
    object: str
    region: str


@dataclass
class Totals:
    code: int = 0
    code_inc_data: int = 0
    ro_data: int = 0
    rw_data: int = 0
    zi_data: int = 0
    debug: int = 0
    total_ro: int = 0
    total_rw: int = 0
    total_rom: int = 0
    # compressed RW in ELF image (often much smaller than full RW)
    elf_rw_compressed: int | None = None


@dataclass
class MapReport:
    map_path: str
    tool: str = ""
    regions: list[Region] = field(default_factory=list)
    objects: list[ComponentSize] = field(default_factory=list)
    libraries: list[ComponentSize] = field(default_factory=list)
    library_members: list[ComponentSize] = field(default_factory=list)
    sections: list[SectionEntry] = field(default_factory=list)
    totals: Totals = field(default_factory=Totals)
    categories: dict[str, ComponentSize] = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Category rules (first match wins)
# ---------------------------------------------------------------------------

CATEGORY_RULES: list[tuple[str, re.Pattern[str]]] = [
    ("HAL", re.compile(r"^stm32h7xx_hal", re.I)),
    ("Startup/System", re.compile(r"^(startup_|system_stm32|stm32h7xx_it|stm32h7xx_hal_msp)", re.I)),
    ("FreeRTOS", re.compile(
        r"^(tasks|queue|list|timers|heap_\d|port|cmsis_os2|freertos|croutine|"
        r"event_groups|stream_buffer)\.o$",
        re.I,
    )),
    ("SEGGER", re.compile(r"^segger_", re.I)),
    ("Shell", re.compile(r"^shell", re.I)),
    ("SFUD", re.compile(r"^sfud", re.I)),
    ("LittleFS", re.compile(r"^lfs", re.I)),
    ("TFT/GFX", re.compile(
        r"^(adafruit_|tft_|mjc_|arduino|print|spiwrapper|buffered_display)",
        re.I,
    )),
    ("BSP", re.compile(
        r"^(gpio|dma|spi|usart|rtt|uart_async|spi_flash|multi_button|"
        r"log|sys_log|sys_time|cpu_cmd)\.o$",
        re.I,
    )),
    ("App", re.compile(r"^(main|common_def)\.o$", re.I)),
    ("C Runtime", re.compile(r"\.(l|lib)$|c_w\.l|fz_wv|m_wv|libcpp", re.I)),
]


def categorize(name: str) -> str:
    base = name
    # strip path-like library wrappers: c_w.l(__printf.o) → handle as library
    m = re.match(r"(.+\.l)\((.+)\)", name)
    if m:
        return "C Runtime"
    for cat, pat in CATEGORY_RULES:
        if pat.search(base):
            return cat
    return "Other"


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

RE_LOAD_REGION = re.compile(
    r"Load Region\s+(\S+)\s+\("
    r"Base:\s*(0x[0-9a-fA-F]+),\s*"
    r"Size:\s*(0x[0-9a-fA-F]+),\s*"
    r"Max:\s*(0x[0-9a-fA-F]+)"
    r"([^)]*)\)"
)

RE_EXEC_REGION = re.compile(
    r"Execution Region\s+(\S+)\s+\("
    r"Exec base:\s*(0x[0-9a-fA-F]+),\s*"
    r"Load base:\s*(0x[0-9a-fA-F]+|[-]),\s*"
    r"Size:\s*(0x[0-9a-fA-F]+),\s*"
    r"Max:\s*(0x[0-9a-fA-F]+)"
    r"([^)]*)\)"
)

RE_COMPRESSED = re.compile(r"COMPRESSED\[(0x[0-9a-fA-F]+)\]")

# Component size row (6 numbers + name):
#       4712         12       1432          0          0     107016   adafruit_gfx.o
RE_COMPONENT = re.compile(
    r"^\s*(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(\S.+?)\s*$"
)

RE_TOTAL_LINE = re.compile(
    r"Total\s+(RO|RW|ROM)\s+Size[^\d]*(\d+)\s*\(\s*([\d.]+)\s*kB\s*\)",
    re.I,
)


def _hex(s: str) -> int:
    if s in ("-", "", "COMPRESSED"):
        return 0
    return int(s, 16)


def _parse_load_addr(s: str) -> int | None:
    """Load address field may be hex, '-' (ZI/no load), or 'COMPRESSED'."""
    if s in ("-", ""):
        return None
    if s == "COMPRESSED":
        return None  # compressed into load region; no fixed load addr per section
    return int(s, 16)


def parse_map(path: Path) -> MapReport:
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    report = MapReport(map_path=str(path))

    if lines:
        report.tool = lines[0].strip()

    # --- regions + sections ---
    in_memmap = False
    current_exec_region: str | None = None
    section_header_seen = False

    for line in lines:
        if line.startswith("Memory Map of the image"):
            in_memmap = True
            continue
        if in_memmap and line.startswith("Image component sizes"):
            in_memmap = False
            current_exec_region = None
            continue

        if not in_memmap:
            continue

        m = RE_LOAD_REGION.search(line)
        if m:
            attrs = m.group(5)
            comp = RE_COMPRESSED.search(attrs)
            report.regions.append(
                Region(
                    name=m.group(1),
                    kind="load",
                    base=_hex(m.group(2)),
                    size=_hex(m.group(3)),
                    max_size=_hex(m.group(4)),
                    compressed=_hex(comp.group(1)) if comp else None,
                    attrs=attrs.strip(" ,"),
                )
            )
            continue

        m = RE_EXEC_REGION.search(line)
        if m:
            load_base_s = m.group(3)
            attrs = m.group(6)
            comp = RE_COMPRESSED.search(attrs)
            region = Region(
                name=m.group(1),
                kind="exec",
                base=_hex(m.group(2)),
                size=_hex(m.group(4)),
                max_size=_hex(m.group(5)),
                load_base=None if load_base_s == "-" else _hex(load_base_s),
                compressed=_hex(comp.group(1)) if comp else None,
                attrs=attrs.strip(" ,"),
            )
            report.regions.append(region)
            current_exec_region = region.name
            section_header_seen = False
            continue

        if "Exec Addr" in line and "Load Addr" in line and "Size" in line:
            section_header_seen = True
            continue

        if (
            section_header_seen
            and current_exec_region
            and line.strip()
            and not line.strip().startswith("====")
        ):
            # skip blank / separator
            parts = line.split()
            if len(parts) < 3:
                continue
            try:
                exec_addr = _hex(parts[0])
                load_s = parts[1]
                size = _hex(parts[2])
            except ValueError:
                continue

            typ = parts[3] if len(parts) > 3 else ""
            load_addr = _parse_load_addr(load_s)

            if typ == "PAD":
                report.sections.append(
                    SectionEntry(
                        exec_addr=exec_addr,
                        load_addr=load_addr,
                        size=size,
                        typ="PAD",
                        attr="",
                        name="PAD",
                        object="",
                        region=current_exec_region,
                    )
                )
                continue

            attr = parts[4] if len(parts) > 4 else ""
            # remaining: optional idx, section name (may have *), object
            rest = parts[5:]
            # drop leading integer index if present
            if rest and rest[0].isdigit():
                rest = rest[1:]
            if not rest:
                continue
            # last token is often object; section name is everything before
            if len(rest) == 1:
                sec_name, obj = rest[0].lstrip("*"), ""
            else:
                obj = rest[-1]
                sec_name = " ".join(rest[:-1]).lstrip("* ").strip()
            report.sections.append(
                SectionEntry(
                    exec_addr=exec_addr,
                    load_addr=load_addr,
                    size=size,
                    typ=typ,
                    attr=attr,
                    name=sec_name,
                    object=obj,
                    region=current_exec_region,
                )
            )

    # --- image component sizes ---
    # Three tables: Object Name / Library Member Name / Library Name / then Grand Totals
    mode: str | None = None  # object | member | library | grand
    in_component = False

    for line in lines:
        if line.startswith("Image component sizes"):
            in_component = True
            mode = None
            continue
        if not in_component:
            continue

        if "Object Name" in line:
            mode = "object"
            continue
        if "Library Member Name" in line:
            mode = "member"
            continue
        if "Library Name" in line and "Member" not in line:
            mode = "library"
            continue

        mtot = RE_TOTAL_LINE.search(line)
        if mtot:
            kind = mtot.group(1).upper()
            val = int(mtot.group(2))
            if kind == "RO":
                report.totals.total_ro = val
            elif kind == "RW":
                report.totals.total_rw = val
            elif kind == "ROM":
                report.totals.total_rom = val
            continue

        m = RE_COMPONENT.match(line)
        if not m:
            # detect grand totals by name even if mode is unclear
            if "Grand Totals" in line:
                gm = RE_COMPONENT.match(line)
                # fall through handled below via name
            continue

        code = int(m.group(1))
        inc = int(m.group(2))
        ro = int(m.group(3))
        rw = int(m.group(4))
        zi = int(m.group(5))
        dbg = int(m.group(6))
        name = m.group(7).strip()

        # skip subtotal / padding annotation rows
        if name.startswith("(incl."):
            continue
        if name in ("Object Totals", "Library Totals"):
            continue

        comp = ComponentSize(
            name=name,
            code=code,
            code_inc_data=inc,
            ro_data=ro,
            rw_data=rw,
            zi_data=zi,
            debug=dbg,
        )

        if "Grand Totals" in name:
            report.totals.code = code
            report.totals.code_inc_data = inc
            report.totals.ro_data = ro
            report.totals.rw_data = rw
            report.totals.zi_data = zi
            report.totals.debug = dbg
            continue
        if "ELF Image Totals" in name:
            report.totals.elf_rw_compressed = rw
            continue
        if "ROM Totals" in name:
            continue

        if mode == "object":
            comp.kind = "object"
            report.objects.append(comp)
        elif mode == "member":
            comp.kind = "library_member"
            report.library_members.append(comp)
        elif mode == "library":
            comp.kind = "library"
            report.libraries.append(comp)

    # category aggregation (objects + libraries as C Runtime lump)
    cats: dict[str, ComponentSize] = {}
    for obj in report.objects:
        cat = categorize(obj.name)
        if cat not in cats:
            cats[cat] = ComponentSize(name=cat, kind="category")
        c = cats[cat]
        c.code += obj.code
        c.code_inc_data += obj.code_inc_data
        c.ro_data += obj.ro_data
        c.rw_data += obj.rw_data
        c.zi_data += obj.zi_data
        c.debug += obj.debug
    for lib in report.libraries:
        cat = "C Runtime"
        if cat not in cats:
            cats[cat] = ComponentSize(name=cat, kind="category")
        c = cats[cat]
        c.code += lib.code
        c.code_inc_data += lib.code_inc_data
        c.ro_data += lib.ro_data
        c.rw_data += lib.rw_data
        c.zi_data += lib.zi_data
        c.debug += lib.debug
    report.categories = cats

    return report


# ---------------------------------------------------------------------------
# Formatting
# ---------------------------------------------------------------------------


def human_bytes(n: int) -> str:
    if n < 0:
        return f"-{human_bytes(-n)}"
    if n < 1024:
        return f"{n} B"
    if n < 1024 * 1024:
        return f"{n / 1024:.2f} KB"
    return f"{n / (1024 * 1024):.2f} MB"


def bar(pct: float, width: int = 24) -> str:
    pct = max(0.0, min(100.0, pct))
    filled = int(round(width * pct / 100.0))
    return "[" + "#" * filled + "-" * (width - filled) + "]"


def usage_color_label(pct: float) -> str:
    if pct >= 95:
        return "CRITICAL"
    if pct >= 85:
        return "HIGH"
    if pct >= 70:
        return "WARN"
    return "OK"


def print_header(title: str) -> None:
    print()
    print("=" * 78)
    print(f"  {title}")
    print("=" * 78)


def print_regions(report: MapReport) -> None:
    print_header("Memory Regions")
    print(
        f"{'Region':<22} {'Kind':<5} {'Base':>12} {'Used':>12} {'Max':>12} "
        f"{'Free':>12} {'%':>7}  Status"
    )
    print("-" * 100)
    for r in report.regions:
        used = r.compressed if (r.kind == "load" and r.compressed is not None) else r.size
        # For load region, armlink reports uncompressed Size and COMPRESSED[actual]
        # Flash occupancy for load = compressed if present else size
        free = max(0, r.max_size - used) if r.max_size else 0
        pct = 100.0 * used / r.max_size if r.max_size else 0.0
        note = ""
        if r.compressed is not None and r.kind == "load":
            note = f"  (uncomp={human_bytes(r.size)}, compressed={human_bytes(r.compressed)})"
        elif r.compressed is not None:
            note = f"  (comp={human_bytes(r.compressed)})"
        print(
            f"{r.name:<22} {r.kind:<5} 0x{r.base:08X} {human_bytes(used):>12} "
            f"{human_bytes(r.max_size):>12} {human_bytes(free):>12} "
            f"{pct:6.1f}%  {usage_color_label(pct):<8} {bar(pct, 16)}{note}"
        )


def print_flash_summary(report: MapReport) -> None:
    print_header("Flash / ROM Summary")
    t = report.totals
    # Prefer ER_IROM1 / flash exec region if present
    flash_regions = [
        r
        for r in report.regions
        if r.kind == "exec" and (r.name.startswith("ER_IROM") or "IROM" in r.name or r.base == 0x08000000)
    ]
    load_flash = [
        r
        for r in report.regions
        if r.kind == "load" and (r.name.startswith("LR_IROM") or r.base == 0x08000000)
    ]

    if t.total_rom:
        print(f"  Total RO  (Code + RO Data)     : {t.total_ro:>8}  ({human_bytes(t.total_ro)})")
        print(f"  Total RW  (RW + ZI runtime)    : {t.total_rw:>8}  ({human_bytes(t.total_rw)})")
        print(f"  Total ROM (Code+RO+RW init)    : {t.total_rom:>8}  ({human_bytes(t.total_rom)})")
        print(
            f"  Breakdown Code / RO / RW init  : "
            f"{t.code} / {t.ro_data} / "
            f"{t.elf_rw_compressed if t.elf_rw_compressed is not None else t.rw_data}"
        )

    if flash_regions:
        r = flash_regions[0]
        print()
        print(f"  Execution region {r.name}:")
        print(f"    Base  : 0x{r.base:08X}")
        print(f"    Used  : {r.size} ({human_bytes(r.size)})")
        print(f"    Max   : {r.max_size} ({human_bytes(r.max_size)})")
        print(f"    Free  : {r.free} ({human_bytes(r.free)})")
        print(f"    Usage : {r.usage_pct:.1f}%  {usage_color_label(r.usage_pct)}  {bar(r.usage_pct)}")

    if load_flash:
        r = load_flash[0]
        used = r.compressed if r.compressed is not None else r.size
        free = max(0, r.max_size - used) if r.max_size else 0
        pct = 100.0 * used / r.max_size if r.max_size else 0.0
        print()
        print(f"  Load region {r.name} (actual Flash image size):")
        print(f"    Image : {used} ({human_bytes(used)})"
              + (f"  [uncompressed logical {human_bytes(r.size)}]" if r.compressed else ""))
        print(f"    Max   : {r.max_size} ({human_bytes(r.max_size)})")
        print(f"    Free  : {free} ({human_bytes(free)})")
        print(f"    Usage : {pct:.1f}%  {usage_color_label(pct)}  {bar(pct)}")


def print_table(
    title: str,
    items: list[ComponentSize],
    top: int,
    sort_key: str = "flash",
) -> None:
    print_header(title)
    key_fn = {
        "flash": lambda c: c.flash,
        "ram": lambda c: c.ram,
        "code": lambda c: c.code,
        "ro": lambda c: c.ro,
        "rom": lambda c: c.rom_reported,
    }.get(sort_key, lambda c: c.flash)

    ranked = sorted(items, key=key_fn, reverse=True)
    if top > 0:
        ranked = ranked[:top]

    total_flash = sum(c.flash for c in items) or 1

    print(
        f"{'#':>3}  {'Name':<36} {'Flash*':>10} {'%F':>6} "
        f"{'Code':>8} {'RO':>8} {'RW':>8} {'ZI':>8} {'RAM':>10}"
    )
    print("-" * 110)
    for i, c in enumerate(ranked, 1):
        print(
            f"{i:>3}  {c.name:<36} {human_bytes(c.flash):>10} "
            f"{100.0 * c.flash / total_flash:5.1f}% "
            f"{c.code:>8} {c.ro_data:>8} {c.rw_data:>8} {c.zi_data:>8} "
            f"{human_bytes(c.ram):>10}"
        )

    shown_flash = sum(c.flash for c in ranked)
    print("-" * 110)
    print(
        f"{'':>3}  {'(shown / all Flash*)':<36} "
        f"{human_bytes(shown_flash):>10} / {human_bytes(sum(c.flash for c in items))}"
        f"    RAM total: {human_bytes(sum(c.ram for c in items))}"
    )
    print("  * Flash = Code + RO Data (object-level RW may include UNINIT heap; see Total ROM)")


def print_categories(report: MapReport) -> None:
    cats = list(report.categories.values())
    print_table("Flash by Category", cats, top=0, sort_key="flash")
    total = sum(c.flash for c in cats) or 1
    print()
    print("  Category pie (Flash):")
    for c in sorted(cats, key=lambda x: x.flash, reverse=True):
        pct = 100.0 * c.flash / total
        print(f"    {c.name:<18} {bar(pct, 30)} {pct:5.1f}%  {human_bytes(c.flash)}")


def print_top_sections(report: MapReport, top: int = 30) -> None:
    # Flash sections: typically in ER_IROM* with RO attr or Code type
    # Prefer IROM regions; fall back to any RO section
    irom = [s for s in report.sections if "IROM" in s.region and s.size > 0]
    if irom:
        flash_secs = irom
    else:
        flash_secs = [
            s
            for s in report.sections
            if s.size > 0 and (s.attr == "RO" or s.typ in ("Code", "Data"))
        ]

    ranked = sorted(flash_secs, key=lambda s: s.size, reverse=True)[:top]
    print_header(f"Top {len(ranked)} Flash Sections")
    print(
        f"{'#':>3}  {'Size':>10}  {'Addr':>12}  {'Type':<6} {'Section':<32} Object"
    )
    print("-" * 100)
    for i, s in enumerate(ranked, 1):
        print(
            f"{i:>3}  {human_bytes(s.size):>10}  0x{s.exec_addr:08X}  "
            f"{s.typ:<6} {s.name[:32]:<32} {s.object}"
        )


def print_ram_regions(report: MapReport) -> None:
    ram = [
        r
        for r in report.regions
        if r.kind == "exec" and "IROM" not in r.name and r.max_size > 0
    ]
    if not ram:
        return
    print_header("RAM Execution Regions")
    print(
        f"{'Region':<22} {'Base':>12} {'Used':>12} {'Max':>12} "
        f"{'Free':>12} {'%':>7}  Status"
    )
    print("-" * 90)
    for r in ram:
        print(
            f"{r.name:<22} 0x{r.base:08X} {human_bytes(r.size):>12} "
            f"{human_bytes(r.max_size):>12} {human_bytes(r.free):>12} "
            f"{r.usage_pct:6.1f}%  {usage_color_label(r.usage_pct):<8} {bar(r.usage_pct, 16)}"
        )


def print_hints(report: MapReport) -> None:
    print_header("Optimization Hints (auto)")
    hints: list[str] = []

    flash_exec = [
        r
        for r in report.regions
        if r.kind == "exec" and ("IROM" in r.name or r.base == 0x08000000)
    ]
    if flash_exec and flash_exec[0].usage_pct >= 85:
        free = flash_exec[0].free
        hints.append(
            f"Flash 使用率 {flash_exec[0].usage_pct:.1f}%（剩余 {human_bytes(free)}）。"
            "优先削减体积最大的模块。"
        )

    cats = report.categories
    if cats:
        ranked = sorted(cats.values(), key=lambda c: c.flash, reverse=True)
        top = ranked[0]
        hints.append(f"最大类别: {top.name} = {human_bytes(top.flash)} Flash。")

    # specific module hints
    name_map = {o.name.lower(): o for o in report.objects}
    for key, tip in [
        ("shell.o", "letter-shell 较大，可关未用命令/补全/历史，或改用更轻量 CLI。"),
        ("segger_sysview.o", "SystemView 仅调试需要；Release 可关掉 trace 宏与相关源文件。"),
        ("segger_rtt_printf.o", "RTT printf 体积不小；可用精简日志或编译期剥离。"),
        ("adafruit_gfx.o", "Adafruit GFX 字体/图形 API 占 Flash；可换精简绘制或裁剪字体。"),
        ("sfud.o", "SFUD 含较多字符串/表；可关 SFDP/调试输出。"),
        ("stm32h7xx_hal_dma.o", "HAL DMA 全功能较大；确认只链入需要的 API（LTO/不用的模块）。"),
        ("stm32h7xx_hal_uart.o", "HAL UART 较大；可评估 LL 驱动或精简回调路径。"),
        ("tft_fb.o", "帧缓冲相关 ZI 很大（RAM）；Flash 侧注意只保留必要接口。"),
    ]:
        if key in name_map and name_map[key].flash >= 2048:
            hints.append(f"{key}: {human_bytes(name_map[key].flash)} — {tip}")

    objs_sorted = sorted(report.objects, key=lambda o: o.flash, reverse=True)[:5]
    if objs_sorted:
        hints.append(
            "Flash(Code+RO) Top5: "
            + ", ".join(f"{o.name}({human_bytes(o.flash)})" for o in objs_sorted)
        )

    # Large RW that is likely heap/buffers (not true flash cost)
    big_rw = [o for o in report.objects if o.rw_data >= 4096]
    for o in sorted(big_rw, key=lambda x: x.rw_data, reverse=True)[:3]:
        hints.append(
            f"{o.name}: RW={human_bytes(o.rw_data)}（多为 heap/缓冲，"
            f"实际 Flash 约 {human_bytes(o.flash)} Code+RO；勿按 RW 估 Flash）。"
        )

    # printf / floating point from library members
    fp_members = [
        m
        for m in report.library_members
        if any(x in m.name for x in ("_printf_fp", "btod", "bigflt", "printf"))
        and m.flash > 0
    ]
    if fp_members:
        fp_total = sum(m.flash for m in fp_members)
        if fp_total >= 1024:
            hints.append(
                f"C 库 printf/浮点相关约 {human_bytes(fp_total)}。"
                "可改用精简 print、禁用 %f、或 microlib。"
            )

    if not hints:
        hints.append("未发现明显告警；可用 --top / --sections 继续下钻。")

    for i, h in enumerate(hints, 1):
        print(f"  {i}. {h}")


def report_to_dict(report: MapReport) -> dict:
    return {
        "map_path": report.map_path,
        "tool": report.tool,
        "totals": asdict(report.totals),
        "regions": [asdict(r) for r in report.regions],
        "objects": [
            {**asdict(o), "flash": o.flash, "ram": o.ram, "category": categorize(o.name)}
            for o in sorted(report.objects, key=lambda x: x.flash, reverse=True)
        ],
        "libraries": [
            {**asdict(o), "flash": o.flash, "ram": o.ram} for o in report.libraries
        ],
        "categories": {
            k: {**asdict(v), "flash": v.flash, "ram": v.ram}
            for k, v in report.categories.items()
        },
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

DEFAULT_MAP_CANDIDATES = [
    Path("MDK-ARM/BaseFramework/BaseFramework.map"),
    Path("MDK-ARM") / "BaseFramework" / "BaseFramework.map",
]


def find_default_map() -> Path | None:
    # from CWD and from script-relative project root
    here = Path.cwd()
    script_root = Path(__file__).resolve().parents[2]  # .../BaseFramework
    for base in (here, script_root):
        for rel in DEFAULT_MAP_CANDIDATES:
            p = base / rel
            if p.is_file():
                return p
        # also search one level of MDK-ARM/*/*.map
        mdk = base / "MDK-ARM"
        if mdk.is_dir():
            maps = list(mdk.glob("*/*.map"))
            if maps:
                return maps[0]
    return None


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Analyze ARM armlink .map for Flash/RAM usage (Keil MDK).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
examples:
  uv run analyze_map.py
  uv run analyze_map.py path/to/foo.map --top 40
  uv run analyze_map.py --category --sections
  uv run analyze_map.py --json > report.json
""",
    )
    p.add_argument(
        "map",
        nargs="?",
        type=Path,
        help="Path to .map file (default: MDK-ARM/**/*.map under project root)",
    )
    p.add_argument("--top", type=int, default=25, help="Top N objects (default 25, 0=all)")
    p.add_argument("--category", action="store_true", help="Show category breakdown")
    p.add_argument("--sections", action="store_true", help="Show largest Flash sections")
    p.add_argument("--ram", action="store_true", help="Show RAM region usage")
    p.add_argument("--libraries", action="store_true", help="Show library totals")
    p.add_argument("--members", action="store_true", help="Show library member ranking")
    p.add_argument("--all", action="store_true", help="Full report")
    p.add_argument("--json", action="store_true", help="JSON output")
    p.add_argument(
        "--sort",
        choices=("flash", "ram", "code", "ro", "rom"),
        default="flash",
        help="Object table sort key (default: flash=Code+RO; rom=Code+RO+RW)",
    )
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_argparser().parse_args(argv)
    map_path = args.map or find_default_map()
    if map_path is None:
        print("ERROR: no .map file found. Pass path explicitly.", file=sys.stderr)
        return 1
    map_path = map_path.resolve()
    if not map_path.is_file():
        print(f"ERROR: map file not found: {map_path}", file=sys.stderr)
        return 1

    report = parse_map(map_path)

    if args.json:
        print(json.dumps(report_to_dict(report), indent=2, ensure_ascii=False))
        return 0

    print_header("MAP Analyzer")
    print(f"  File : {map_path}")
    print(f"  Tool : {report.tool or '(unknown)'}")
    print(f"  Objects: {len(report.objects)}  Libraries: {len(report.libraries)}  "
          f"Sections: {len(report.sections)}")

    # default view: summary + regions + top objects + categories + hints
    show_all = args.all
    print_flash_summary(report)
    print_regions(report)

    if show_all or args.ram or True:
        # always show RAM regions — cheap and useful
        print_ram_regions(report)

    if show_all or args.category or True:
        print_categories(report)

    print_table(
        f"Top Objects by {args.sort.upper()}",
        report.objects,
        top=args.top,
        sort_key=args.sort,
    )

    if show_all or args.libraries:
        print_table("Libraries", report.libraries, top=0, sort_key=args.sort)

    if show_all or args.members:
        print_table(
            "Library Members",
            report.library_members,
            top=args.top if args.top else 30,
            sort_key=args.sort,
        )

    if show_all or args.sections:
        print_top_sections(report, top=args.top if args.top else 30)

    print_hints(report)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
