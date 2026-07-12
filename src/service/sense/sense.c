#include "sense.h"

static sense_mode_t g_sense_mode = SENSE_MODE_ADC;

exit_code_t sense_init(sense_mode_t mode) {
    g_sense_mode = mode;
    exit_code_t ret = bsp_adc_init();
    return ret;
}

float sense_get_voltage(void) {
	uint16_t raw_value = bsp_adc_get_raw_value(SENSE_VOLTAGE_CHANNEL);
    // Assuming a 16-bit ADC and a reference voltage of 3.3V
    return ((raw_value / SENSE_RESOLUTION) * SENSE_VOLTAGE_REF) * SENSE_VOLTAGE_FACTOR;
}

float sense_get_current(void) {
    switch (g_sense_mode) {
        case SENSE_MODE_ADC:
            uint16_t raw_value = bsp_adc_get_raw_value(SENSE_CURRENT_CHANNEL);
            // Assuming a 16-bit ADC and a reference voltage of 3.3V
            return ((raw_value / SENSE_RESOLUTION) * SENSE_VOLTAGE_REF) * SENSE_CURRENT_FACTOR;
        case SENSE_MODE_PAG2ADC:
            // Implement PAG2ADC mode if needed
            return 0.0f; // Placeholder
        default:
            return 0.0f; // Invalid mode
    }

}