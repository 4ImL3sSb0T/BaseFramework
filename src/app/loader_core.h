#ifndef LOADER_CORE_H
#define LOADER_CORE_H

/* fsm + mode + control；唯一写执行器 — 见 LOADER_DESIGN.md */
#include "loader_runtime.h"
#include "service/sense/sense.h"
#include "service/load/load_out.h"
#include "common/tools/common_def.h"
#include "common/pid/pid.h"

exit_code_t loader_core_init(void);
void loader_core_control_update(void);

#endif /* LOADER_CORE_H */
