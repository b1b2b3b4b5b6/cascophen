#ifndef FAN_H
#define FAN_H

#include "std_port_common.h"
#include "std_nvs.h"

void fan_init();
void fan_led_set(int status);
void fan_led_restore();
#endif