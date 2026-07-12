#include "sense.h"

float sense_get_voltage(void) {
    uint16_t raw_value = bsp_adc_get_raw_value(SENSE_VOLTAGE_CHANNEL);
    // Assuming a 16-bit ADC and a reference voltage of 3.3V
    return ((raw_value / SENSE_RESOLUTION) * SENSE_VOLTAGE_REF) * SENSE_VOLTAGE_FACTOR;
}

float sense_get_current(void) {
    uint16_t raw_value = bsp_adc_get_raw_value(SENSE_CURRENT_CHANNEL);
    // Assuming a 16-bit ADC and a reference voltage of 3.3V
    return ((raw_value / SENSE_RESOLUTION) * SENSE_VOLTAGE_REF) * SENSE_CURRENT_FACTOR;
}