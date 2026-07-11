#include "stdint.h"

typedef struct{
    float kp;
    float ki;
    float kd;
    float current_setpoint;
    float current_measurement;
} loader_control_t;