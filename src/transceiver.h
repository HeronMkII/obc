// AVR Library Includes
#include <avr/io.h>
#include <stdio.h>
#include <stdbool.h>

// lib-common includes
#include <uart/uart.h>
#include <utilities/utilities.h>


//Default Address - DO NOT CHANGE
#define TRANS_ADDR  0x22

// Status control register bits
#define TRANS_UART_BAUD 12  // bits 13-12
#define TRANS_RESET     11
#define TRANS_RF_MODE   8   // bits 10-8
#define TRANS_ECHO      7
#define TRANS_BCN       6
#define TRANS_PIPE      5
#define TRANS_BOOT      4
#define TRANS_CTS       3
#define TRANS_SEC       2
#define TRANS_FRAM      1
#define TRANS_RFTS      0

/*
Default status register value

baud rate = 0b00 (9600)
reset = 0b0 (not reset)
RF Mode = 0b011 (mode 3 - 2GFSK, 9600 bps data rate, 2400 Hz Fdev, 0.5 ModInd)
echo = 0b0 (off)
beacon = 0b0 (off)
pipe = 0b0 (off)
boot = 0b0 (application mode)
*/
#define TRANS_DEF_SCW   0x0303

// Default frequency
#define TRANS_DEF_FREQ 0x9DD80942UL //default 437 MHz freq

// Number of characters in a call sign (NOT INCLUDING '\0' termination)
// Need to add 1 to array sizes for '\0' terminating character
#define TRANS_CALL_SIGN_LEN 6

// Maximum number of times to attempt each command
// TODO - what number?
#define TRANS_MAX_CMD_ATTEMPTS 3


// Helper Functions to process responses
uint8_t char_to_hex(uint8_t c);
uint32_t scan_uint(volatile uint8_t* string, uint8_t offset, uint8_t count);
uint8_t string_cmp(volatile uint8_t* first, char* second, uint8_t len);
uint8_t valid_cmd_response(uint8_t expected_len);
uint8_t wait_for_cmd_response(uint16_t *timeout_left);

// Initialization
void init_trans(void);
uint8_t trans_uart_rx_cb(const uint8_t* buf, uint8_t len);

// 1
uint8_t set_trans_scw(uint16_t scw);
uint8_t get_trans_scw(uint8_t* rssi, uint8_t* reset_count, uint16_t* scw);
uint8_t set_trans_scw_bit(uint8_t bit_index, uint8_t value);
uint8_t reset_trans(void);
uint8_t set_trans_rf_mode(uint8_t mode);
uint8_t turn_on_trans_echo(void);
uint8_t turn_off_trans_echo(void);
uint8_t turn_on_trans_beacon(void);
uint8_t turn_off_trans_beacon(void);
uint8_t turn_on_trans_pipe(void);

// 2
uint8_t set_trans_freq(uint32_t freq);
uint8_t get_trans_freq(uint8_t* rssi, uint32_t* freq);

// 4
uint8_t set_trans_pipe_timeout(uint8_t timeout);
uint8_t get_trans_pipe_timeout(uint8_t* rssi, uint8_t* timeout);

// 5
uint8_t set_trans_beacon_period(uint16_t period);
uint8_t get_trans_beacon_period(uint8_t* rssi, uint16_t* period);

// 7
uint8_t set_trans_dest_call_sign(char* call_sign);
uint8_t get_trans_dest_call_sign(char* call_sign);

// 8
uint8_t set_trans_src_call_sign(char* call_sign);
uint8_t get_trans_src_call_sign(char* call_sign);

// 16
uint8_t get_trans_uptime(uint8_t* rssi, uint32_t* uptime);

// 17
uint8_t get_trans_num_tx_packets(uint8_t* rssi, uint32_t* num_tx_packets);

// 18
uint8_t get_trans_num_rx_packets(uint8_t* rssi, uint32_t* num_rx_packets);

// 19
uint8_t get_trans_num_rx_packets_crc(uint8_t* rssi, uint32_t* num_rx_packets_crc);