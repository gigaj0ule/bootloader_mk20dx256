/*
 * led_functions.h
 *
 * Created: 10/21/2018 3:37:02 PM
 * Author: Adam Munich
 */ 

#include "led_functions.h"
#include "core_pins.h"

void led_init()
{
	pinMode(LED_PIN, OUTPUT);
// 	// Set the status LED on PC5, as an indication that we're in bootloading mode.
// 	PORTC_PCR5 = PORT_PCR_MUX(1) | PORT_PCR_DSE | PORT_PCR_SRE;
// 	GPIOC_PDDR = led_bit;
// 	GPIOC_PDOR = led_bit;
}

void led_toggle()
{
	digitalWriteFast(LED_PIN, !digitalReadFast(LED_PIN));	
//	GPIOC_PTOR = led_bit;
}

void led_set()
{
	digitalWriteFast(LED_PIN, HIGH);
//	GPIOC_PSOR = led_bit;
}

void led_clear()
{
	digitalWriteFast(LED_PIN, LOW);
//	GPIOC_PCOR = led_bit;
}


void led_pulse_pattern_low(void)
{
	while (1) {
		led_set();
		for (long j = 100000; j; --j) {
			__asm__ volatile ("nop");
		}
		led_clear();
		for (long j = 1000000; j; --j) {
			__asm__ volatile ("nop");
		}
	}
};

void led_pulse_pattern_high(void)
{
	while (1) {
		led_set();
		for (long j = 10000000; j; --j) {
			__asm__ volatile ("nop");
		}
		led_clear();
		for (long j = 1000000; j; --j) {
			__asm__ volatile ("nop");
		}
	}
};


void led_pulse_pattern_regular_slow(void)
{
	while (1) {
		led_toggle();
		for (long j = 10000000; j; --j)
		{
			__asm__ volatile ("nop");
		}
	}
};

void led_pulse_pattern_regular_fast(void)
{
	while (1) {
		led_toggle();
		for (long j = 1000000; j; --j)
		{
			__asm__ volatile ("nop");
		}
	}
};