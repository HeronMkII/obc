// AVR Library Includes
#include    <avr/io.h>
#define F_CPU 8000000UL
#include    <util/delay.h>
#include <stdio.h>
#include <stdbool.h>


// lib-common includes
#include    <uart/uart.h>

#define FREQ 0x9DD80942UL //default 437MHz freq

void init_trans(void);
uint8_t trans_cb(const uint8_t* buf, uint8_t len);
uint8_t char_hex_to_dec(char c);
uint32_t scan_uint(char* string, uint8_t offset, uint8_t count);
void set_trans_scw(uint16_t reg);
uint16_t read_trans_scw();
uint8_t set_trans_freq();
uint16_t read_trans_freq();
uint8_t set_trans_addr(uint8_t addr);
void set_trans_pipe_timeout(uint8_t timeout);
