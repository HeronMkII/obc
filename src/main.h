#ifndef MAIN_H
#define MAIN_H

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <stdint.h>
#include <stdbool.h>
#include <util/delay.h>

#include <uart/uart.h>
#include <timer/timer.h>
#include <can/can.h>
#include <can/can_ids.h>

#include "tx_callbacks.h"
#include "rx_callbacks.h"
#include "timer_callbacks.h"

#define PAY_CMD_TX_MOB 3
#define EPS_CMD_TX_MOB 4
#define DATA_RX_MOB 5

uint8_t next_pay_hk_field_num;
bool send_next_pay_hk_field_num;

uint8_t next_pay_sci_field_num;
bool send_next_pay_sci_field_num;

uint8_t next_eps_hk_field_num;
bool send_next_eps_hk_field_num;

void print_bytes(uint8_t *data, uint8_t len);

#endif
