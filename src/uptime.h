#ifndef UPTIME_H
#define UPTIME_H

#include <stdint.h>

#include <avr/eeprom.h>

#include <timer/timer.h>
#include <uart/uart.h>

#include "rtc.h"

// Default double word (4-byte) value in EEPROM
// TODO - put in lib-common/utilities
#define EEPROM_DEF_DWORD 0xFFFFFFFF

// EEPROM address for storing number of resets
#define RESTART_COUNT_EEPROM_ADDR ((uint32_t*) 0x60)

// Number of seconds between timer callbacks
#define UPTIME_TIMER_PERIOD 1

// Number of functions to be called from the same timer
#define UPTIME_NUM_CALLBACKS 5

typedef void(*uptime_fn_t)(void);

extern uint32_t restart_count;
extern rtc_date_t restart_date;
extern rtc_time_t restart_time;

extern volatile uint32_t uptime_s;

void init_uptime(rtc_date_t date, rtc_time_t time);
void update_restart_count(void);
uint8_t add_uptime_callback(uptime_fn_t callback);

#endif
