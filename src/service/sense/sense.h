#include "bsp/adc/bsp_adc.h"
#include "common/tools/common_def.h"

#define SENSE_VOLTAGE_REF 3.3f
#define SENSE_RESOLUTION 16383.0f // 2^16 - 1 for a 16-bit ADC
#define SENSE_VOLTAGE_CHANNEL 0
#define SENSE_CURRENT_CHANNEL 1

#define SENSE_VOLTAGE_FACTOR 0.05f // Adjust this factor based on your voltage divider
#define SENSE_CURRENT_FACTOR 2.0f // Adjust this factor based on your current sensor

typedef enum {
    SENSE_MODE_ADC,
    SENSE_MODE_PAG2ADC
} sense_mode_t;

exit_code_t sense_init(sense_mode_t mode);
float sense_get_voltage(void);
float sense_get_current(void);