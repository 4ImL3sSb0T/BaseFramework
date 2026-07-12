#include "bsp/adc/bsp_adc.h"

#define SENSE_VOLTAGE_REF 3.3f
#define SENSE_RESOLUTION 16383.0f // 2^16 - 1 for a 16-bit ADC
#define SENSE_VOLTAGE_CHANNEL 0
#define SENSE_CURRENT_CHANNEL 1

#define SENSE_VOLTAGE_FACTOR 0.05f // Adjust this factor based on your voltage divider
#define SENSE_CURRENT_FACTOR 2.0f // Adjust this factor based on your current sensor

float sense_get_voltage(void);
float sense_get_current(void);