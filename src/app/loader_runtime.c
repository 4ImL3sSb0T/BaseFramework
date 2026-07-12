#include "loader_runtime.h"
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t g_loader_runtime_mutex = NULL;
static loader_runtime_t g_loader_runtime = {
    .state = LOADER_STATE_IDLE,
    .error = LOADER_ERROR_NONE,
    .mode = LOADER_MODE_CC,
    .current_setpoint = 0.0f,
    .current_measurement = 0.0f,
    .voltage_setpoint = 0.0f,
    .voltage_measurement = 0.0f,
    .temperature_measurement = 0.0f,
    .temperature_setpoint = 0.0f
};

exit_code_t loader_runtime_init(void) {
    g_loader_runtime_mutex = xSemaphoreCreateMutex();
    if (!g_loader_runtime_mutex) {
        return EXIT_FAIL; // Failed to create mutex
    }
	return EXIT_OK;
}

loader_runtime_t loader_runtime_get() {
	xSemaphoreTake(g_loader_runtime_mutex, portMAX_DELAY);
	loader_runtime_t runtime = g_loader_runtime;
	xSemaphoreGive(g_loader_runtime_mutex);
	return runtime;
}

exit_code_t loader_runtime_set(loader_runtime_t *runtime) {
    xSemaphoreTake(g_loader_runtime_mutex, portMAX_DELAY);
	g_loader_runtime = *runtime;
    xSemaphoreGive(g_loader_runtime_mutex);
	return EXIT_OK;
}
