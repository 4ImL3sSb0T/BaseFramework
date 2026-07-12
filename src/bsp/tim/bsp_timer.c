#include "bsp_timer.h"

static timer_callback_t g_timer_callback = NULL;

exit_code_t bsp_timer_init(void) {
    // Initialize the timer here
    // For example, configure TIM16 for a specific frequency and mode
    HAL_TIM_Base_Start_IT(&htim16);
    return EXIT_OK;
}

exit_code_t bsp_timer_register_callback(timer_callback_t callback) {
    // Register the callback function to be called on timer interrupt
    g_timer_callback = callback;
    return EXIT_OK;
}

exit_code_t bsp_timer_handler(void) {
    if (g_timer_callback != NULL) {
        g_timer_callback();
    } else {
        return EXIT_ERROR; // No callback registered
    }
    return EXIT_OK;
}