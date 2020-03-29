#ifndef HEATER_H
#define HEATER_H

#include "std_port_common.h"

void heater_init();
void heater_led_set(int status);
void heater_led_restore();

#endif