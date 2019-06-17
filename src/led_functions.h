/*
 * led_functions.h
 *
 * Created: 10/21/2018 3:36:15 PM
 *  Author: root
 */ 


#ifndef LED_FUNCTIONS_H_
#define LED_FUNCTIONS_H_

#include "core_pins.h"

//#define LED_PIN 13
#define LED_PIN 21

void led_init(void);
void led_toggle(void);
void led_set(void);
void led_clear(void);

void led_pulse_pattern_low(void);
void led_pulse_pattern_high(void);
void led_pulse_pattern_regular_slow(void);
void led_pulse_pattern_regular_fast(void);

#endif /* LED_FUNCTIONS_H_ */