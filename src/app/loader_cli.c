/**
 * @file loader_cli.c
 * @brief 电子负载串口调试命令（Letter Shell）
 *
 * 约定（与 LOADER_DESIGN.md 一致）：
 * - 只改设定 / 调 core 的 request_* API
 * - 读 runtime 测量；不直接写 DAC / HAL
 * - 风扇经 service/fan 调试
 */

#include "loader_task.h"
#include "loader_core.h"
#include "loader_runtime.h"
#include "loader_config.h"
#include "service/load_out/load_out.h"
#include "service/fan/fan.h"
#include "lib/shell/shell.h"
#include "lib/shell/log/log.h"
#include "lib/tools/common_def.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------------------------------------------------------------------------- */
/* helpers                                                                    */
/* -------------------------------------------------------------------------- */

static const char *cli_state_name(loader_state_t s)
{
    switch (s) {
    case LOADER_STATE_IDLE:    return "IDLE";
    case LOADER_STATE_RUNNING: return "RUNNING";
    case LOADER_STATE_PAUSED:  return "PAUSED";
    case LOADER_STATE_ERROR:   return "ERROR";
    default:                   return "?";
    }
}

static const char *cli_error_name(loader_error_t e)
{
    switch (e) {
    case LOADER_ERROR_NONE:             return "NONE";
    case LOADER_ERROR_OVERCURRENT:      return "OCP";
    case LOADER_ERROR_OVERTEMPERATURE:  return "OTP";
    case LOADER_ERROR_UNDERVOLTAGE:     return "UVP";
    default:                            return "?";
    }
}

static const char *cli_mode_name(loader_mode_t m)
{
    switch (m) {
    case LOADER_MODE_CC: return "CC";
    case LOADER_MODE_CV: return "CV";
    case LOADER_MODE_CP: return "CP";
    case LOADER_MODE_CR: return "CR";
    default:             return "?";
    }
}

static int cli_parse_mode(const char *s, loader_mode_t *out)
{
    char a, b;

    if (s == NULL || out == NULL || s[0] == '\0') {
        return -1;
    }

    a = (char)tolower((unsigned char)s[0]);
    b = (char)tolower((unsigned char)s[1]);

    if (a == 'c' && b == 'c' && s[2] == '\0') {
        *out = LOADER_MODE_CC;
        return 0;
    }
    if (a == 'c' && b == 'v' && s[2] == '\0') {
        *out = LOADER_MODE_CV;
        return 0;
    }
    if (a == 'c' && b == 'p' && s[2] == '\0') {
        *out = LOADER_MODE_CP;
        return 0;
    }
    if (a == 'c' && b == 'r' && s[2] == '\0') {
        *out = LOADER_MODE_CR;
        return 0;
    }
    return -1;
}

static int cli_parse_float(const char *s, float *out)
{
    char *end = NULL;
    double v;

    if (s == NULL || out == NULL || s[0] == '\0') {
        return -1;
    }
    /* strtod more widely available than strtof on embedded libcs */
    v = strtod(s, &end);
    if (end == s || (end != NULL && *end != '\0')) {
        return -1;
    }
    *out = (float)v;
    return 0;
}

static void cli_print_result(const char *cmd, exit_code_t code)
{
    if (code == EXIT_OK) {
        logPrintln("%s: OK", cmd);
    } else {
        logPrintln("%s: FAIL %s (%d)", cmd, error_code_name(code), (int)code);
    }
}

static float cli_active_setpoint(const loader_runtime_t *rt)
{
    switch (rt->mode) {
    case LOADER_MODE_CC: return rt->current_setpoint;
    case LOADER_MODE_CV: return rt->voltage_setpoint;
    case LOADER_MODE_CP: return rt->power_setpoint;
    case LOADER_MODE_CR: return rt->resistance_setpoint;
    default:             return 0.0f;
    }
}

static const char *cli_active_unit(loader_mode_t m)
{
    switch (m) {
    case LOADER_MODE_CC: return "A";
    case LOADER_MODE_CV: return "V";
    case LOADER_MODE_CP: return "W";
    case LOADER_MODE_CR: return "ohm";
    default:             return "";
    }
}

/* -------------------------------------------------------------------------- */
/* commands                                                                   */
/* -------------------------------------------------------------------------- */

/** lhelp — 命令一览 */
int loader_cli_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    logPrintln("loader CLI:");
    logPrintln("  lhelp              this help");
    logPrintln("  lstatus            state / mode / set / meas / limits");
    logPrintln("  lmeas              measurements only");
    logPrintln("  lrun               request RUN (enable output)");
    logPrintln("  lstop              request STOP (disable output)");
    logPrintln("  lclr               clear FAULT -> IDLE");
    logPrintln("  lmode [cc|cv|cp|cr] get/set mode");
    logPrintln("  lset [i|v|p|r] <x> set setpoint (channel optional)");
    logPrintln("  lset <x>           set active-mode setpoint");
    logPrintln("  lout               actuator readback (norm/Iref/en)");
    logPrintln("  lfan [on|off|%%]    fan enable / duty percent 0..100");
    logPrintln("limits: I<=%.2fA V<=%.2fV P<=%.2fW R<=%.1fohm OCP=%.2fA OTP=%.0fC",
               (double)LOADER_CURRENT_MAX,
               (double)LOADER_VOLTAGE_MAX,
               (double)LOADER_POWER_MAX,
               (double)LOADER_RESISTANCE_MAX,
               (double)LOADER_OVERCURRENT_LIMIT,
               (double)LOADER_OVERTEMPERATURE_LIMIT);
    return 0;
}

/** lstatus — 完整运行时快照 */
int loader_cli_status(int argc, char *argv[])
{
    loader_runtime_t rt;
    float set_v;

    (void)argc;
    (void)argv;

    rt = loader_runtime_get();
    set_v = cli_active_setpoint(&rt);

    logPrintln("--- loader status ---");
    logPrintln("state  : %s", cli_state_name(rt.state));
    logPrintln("error  : %s", cli_error_name(rt.error));
    logPrintln("mode   : %s", cli_mode_name(rt.mode));
    logPrintln("set    : I=%.3fA  V=%.3fV  P=%.3fW  R=%.3fohm",
               (double)rt.current_setpoint,
               (double)rt.voltage_setpoint,
               (double)rt.power_setpoint,
               (double)rt.resistance_setpoint);
    logPrintln("active : %.3f %s", (double)set_v, cli_active_unit(rt.mode));
    logPrintln("meas   : I=%.3fA  V=%.3fV  P=%.3fW  R=%.3fohm  T=%.1fC",
               (double)rt.current_measurement,
               (double)rt.voltage_measurement,
               (double)rt.power_measurement,
               (double)rt.resistance_measurement,
               (double)rt.temperature_measurement);
    logPrintln("out    : en=%d  Iref=%.3fA  Vref=%.3fV  norm=%.4f",
               load_out_is_enabled() ? 1 : 0,
               (double)load_out_get_current(),
               (double)load_out_get_ref_voltage(),
               (double)load_out_get());
    logPrintln("fan    : en=%d  speed=%.0f%%  target=%.0f%%",
               fan_is_enabled() ? 1 : 0,
               (double)(fan_get_speed() * 100.0f),
               (double)(fan_get_target_speed() * 100.0f));
    logPrintln("limits : OCP=%.2fA  OTP=%.0fC  Imax=%.2fA",
               (double)LOADER_OVERCURRENT_LIMIT,
               (double)LOADER_OVERTEMPERATURE_LIMIT,
               (double)LOADER_CURRENT_MAX);
    return 0;
}

/** lmeas — 仅测量 */
int loader_cli_meas(int argc, char *argv[])
{
    loader_runtime_t rt = loader_runtime_get();

    (void)argc;
    (void)argv;

    logPrintln("I=%.3fA V=%.3fV P=%.3fW R=%.3fohm T=%.1fC",
               (double)rt.current_measurement,
               (double)rt.voltage_measurement,
               (double)rt.power_measurement,
               (double)rt.resistance_measurement,
               (double)rt.temperature_measurement);
    return 0;
}

/** lrun — 请求进入 RUN */
int loader_cli_run(int argc, char *argv[])
{
    exit_code_t code;

    (void)argc;
    (void)argv;

    code = loader_core_request_run();
    cli_print_result("lrun", code);
    if (code == EXIT_OK) {
        loader_runtime_t rt = loader_runtime_get();
        logPrintln("  state=%s mode=%s set=%.3f%s",
                   cli_state_name(rt.state),
                   cli_mode_name(rt.mode),
                   (double)cli_active_setpoint(&rt),
                   cli_active_unit(rt.mode));
    }
    return (code == EXIT_OK) ? 0 : (int)code;
}

/** lstop — 请求停机 */
int loader_cli_stop(int argc, char *argv[])
{
    exit_code_t code;

    (void)argc;
    (void)argv;

    code = loader_core_request_stop();
    cli_print_result("lstop", code);
    return (code == EXIT_OK) ? 0 : (int)code;
}

/** lclr — 清除故障 */
int loader_cli_clr(int argc, char *argv[])
{
    exit_code_t code;

    (void)argc;
    (void)argv;

    code = loader_core_clear_fault();
    cli_print_result("lclr", code);
    if (code == EXIT_OK) {
        loader_runtime_t rt = loader_runtime_get();
        logPrintln("  state=%s error=%s",
                   cli_state_name(rt.state),
                   cli_error_name(rt.error));
    }
    return (code == EXIT_OK) ? 0 : (int)code;
}

/**
 * lmode [cc|cv|cp|cr]
 * 无参：打印当前模式；有参：切换模式（不自动 run）
 */
int loader_cli_mode(int argc, char *argv[])
{
    loader_mode_t mode;
    loader_runtime_t rt;

    if (argc < 2) {
        rt = loader_runtime_get();
        logPrintln("mode=%s  (use: lmode cc|cv|cp|cr)", cli_mode_name(rt.mode));
        return 0;
    }

    if (cli_parse_mode(argv[1], &mode) != 0) {
        logPrintln("lmode: bad mode '%s' (cc|cv|cp|cr)", argv[1]);
        return -1;
    }

    loader_runtime_set_mode(mode);
    rt = loader_runtime_get();
    logPrintln("lmode: OK -> %s  set=%.3f%s",
               cli_mode_name(rt.mode),
               (double)cli_active_setpoint(&rt),
               cli_active_unit(rt.mode));
    return 0;
}

/**
 * lset [i|v|p|r] <value>
 * lset <value>   — 按当前模式写对应设定
 */
int loader_cli_set(int argc, char *argv[])
{
    float value;
    char ch;
    loader_runtime_t rt;
    const char *ch_arg = NULL;
    const char *val_arg = NULL;

    if (argc == 2) {
        val_arg = argv[1];
        rt = loader_runtime_get();
        switch (rt.mode) {
        case LOADER_MODE_CC: ch = 'i'; break;
        case LOADER_MODE_CV: ch = 'v'; break;
        case LOADER_MODE_CP: ch = 'p'; break;
        case LOADER_MODE_CR: ch = 'r'; break;
        default:
            logPrintln("lset: unknown mode");
            return -1;
        }
    } else if (argc == 3) {
        ch_arg = argv[1];
        val_arg = argv[2];
        if (ch_arg[0] == '\0' || ch_arg[1] != '\0') {
            logPrintln("lset: channel must be i|v|p|r");
            return -1;
        }
        ch = (char)tolower((unsigned char)ch_arg[0]);
    } else {
        logPrintln("usage: lset [i|v|p|r] <value>");
        logPrintln("  i=A  v=V  p=W  r=ohm; omit channel => active mode");
        rt = loader_runtime_get();
        logPrintln("  now mode=%s set=%.3f%s",
                   cli_mode_name(rt.mode),
                   (double)cli_active_setpoint(&rt),
                   cli_active_unit(rt.mode));
        return -1;
    }

    if (cli_parse_float(val_arg, &value) != 0) {
        logPrintln("lset: bad number '%s'", val_arg);
        return -1;
    }

    switch (ch) {
    case 'i':
        loader_runtime_set_current_setpoint(value);
        break;
    case 'v':
        loader_runtime_set_voltage_setpoint(value);
        break;
    case 'p':
        loader_runtime_set_power_setpoint(value);
        break;
    case 'r':
        loader_runtime_set_resistance_setpoint(value);
        break;
    default:
        logPrintln("lset: channel must be i|v|p|r");
        return -1;
    }

    rt = loader_runtime_get();
    logPrintln("lset: OK  I=%.3fA V=%.3fV P=%.3fW R=%.3fohm",
               (double)rt.current_setpoint,
               (double)rt.voltage_setpoint,
               (double)rt.power_setpoint,
               (double)rt.resistance_setpoint);
    return 0;
}

/** lout — 执行器读回（只读） */
int loader_cli_out(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    logPrintln("load_out: en=%d  Iref=%.3fA  Vref=%.3fV  norm=%.4f  (raw scale %u)",
               load_out_is_enabled() ? 1 : 0,
               (double)load_out_get_current(),
               (double)load_out_get_ref_voltage(),
               (double)load_out_get(),
               (unsigned)LOAD_OUT_RESOLUTION);
    return 0;
}

/**
 * lfan
 * lfan on | off
 * lfan <0..100>   百分比，并 enable
 */
int loader_cli_fan(int argc, char *argv[])
{
    float pct;
    exit_code_t code;

    if (argc < 2) {
        logPrintln("fan: en=%d  speed=%.0f%%  target=%.0f%%",
                   fan_is_enabled() ? 1 : 0,
                   (double)(fan_get_speed() * 100.0f),
                   (double)(fan_get_target_speed() * 100.0f));
        logPrintln("usage: lfan on|off|<0..100>");
        return 0;
    }

    if (strcmp(argv[1], "on") == 0) {
        code = fan_enable(true);
        if (code == EXIT_OK && fan_get_target_speed() <= 0.0f) {
            code = fan_set_percent(50.0f);
        }
        cli_print_result("lfan on", code);
        return (code == EXIT_OK) ? 0 : (int)code;
    }

    if (strcmp(argv[1], "off") == 0) {
        code = fan_enable(false);
        cli_print_result("lfan off", code);
        return (code == EXIT_OK) ? 0 : (int)code;
    }

    if (cli_parse_float(argv[1], &pct) != 0) {
        logPrintln("lfan: use on|off|<0..100>");
        return -1;
    }

    if (pct < 0.0f) {
        pct = 0.0f;
    }
    if (pct > 100.0f) {
        pct = 100.0f;
    }

    code = fan_enable(true);
    if (code != EXIT_OK) {
        cli_print_result("lfan enable", code);
        return (int)code;
    }
    code = fan_set_percent(pct);
    cli_print_result("lfan", code);
    if (code == EXIT_OK) {
        logPrintln("  speed=%.0f%%", (double)(fan_get_speed() * 100.0f));
    }
    return (code == EXIT_OK) ? 0 : (int)code;
}
