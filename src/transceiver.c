/*
Transceiver library
EnduroSat UHF Transceiver Type II

Commands are sent through UART (by calling print())
Transceiver responses are handled in trans_uart_rx_cb

Note that all/most configuration functions will return 1 if configuration/read
successful and 0 if not successful

Each of the command functions has the structure of an "attempt" function,
which is repeated until the response is successfully received (with a maximum
number of attempts).

Status Control Register Bits:
15-14: Reserved
13-12: Baudrate = 0 for 9600
11: Reset
10-8: RfMode Choose 0
7: UART Echo Enable (1 = ON 0 = OFF)
6: Beacon Message Enable (1 = ON 0 = OFF)
5: Pipeline Communication Enable
All bits below this point are read only (no writing)
4: Bootloader mode: 1, Application Mode: 0
3/2: CTS/SEC (Reserved)
1: Correct initialization of FRAM (0 = Error)
0: Correct initialization of Radio Transceiver (0 = Error)

Unused features: Beacon contents, packets

TODO - For most of the get commands, OBC misses the first 1 or 2 characters in
    the response for the first attempt, but the second attempt seems to always work

TODO - it was observed that after either sending data in pipe mode, the
    transceiver sent the UART message "+ESTTC<CR>" - figure out what this is

TODO - default status register and frequency?
TODO - macro for repeating command attempts?
TODO - if commands to the transceiver fail, change UART baud rate and switch
    the transceiver back to 9600 baud
TODO - clear cmd_response buffer before sending a command
TODO - for ground station messages, use the first one or two bytes as the
    number of bytes in the message?
*/

#include "transceiver.h"


/* Command response received back from transceiver */
// String - has '\0' termination
volatile uint8_t cmd_response[20];
// Length - number of characters (NOT including '\r' or '\0' termination)
volatile uint8_t cmd_response_len = 0;
// Flag - gets set to true by the UART RX callback when a full command
// (detected by \r termination) is available in `cmd_response`
volatile bool cmd_response_available = false;




/*
Converts the ASCII representation of a hex number to an integer value.
c - character in ASCII format (should be between '0' to '9' or 'A' to 'F')
Returns - value between 0 to 15, or 0 if invalid character
*/
uint8_t char_to_hex(uint8_t c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return 0;
    }
}

/*
Extracts ASCII characters representing a hex number and returns the corresponding integer.
string - string containing ASCII characters of the number to process (volatile
         to prevent compiler warnings with the `cmd_response` buffer)
offset - starting index in the string
count - number of characters (between 1 and 8)
Returns - 32-bit unsigned integer processed
*/
uint32_t scan_uint(volatile uint8_t* string, uint8_t offset, uint8_t count) {
    uint32_t value = 0;
    for (uint8_t i = offset; i < offset + count; i++) {
        value = value << 4;
        value = value + char_to_hex(string[i]);
    }
    return value;
}

/*
Compares strings to see if they are equal
first - first string to compare (volatile to prevent compiler warnings with the
        `cmd_response` buffer)
second - second string to compare
len - number of characters to compare
Returns - 1 if strings are the same, returns 0 otherwise
*/
uint8_t string_cmp(volatile uint8_t* first, char* second, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (first[i] != second[i]) {
            return 0;
        }
    }
    return 1;
}

/*
Check if command was successful by checking characters in `cmd_response`
expected_len - expected length of response (NOT including '\r' character)
Returns - 1 if valid, 0 if invalid

Invalid conditions:
- Length is less than 2
- Does not start with 'OK'
- Length does not match expected length

Otherwise, the command is valid.
*/
uint8_t valid_cmd_response(uint8_t expected_len) {
    // Any response less than 2 characters is invalid
    if (cmd_response_len < 2) {
        return 0;
    }

    // Check if the string starts with "OK"
    char* valid = "OK";
    if (string_cmp(cmd_response, valid, 2) == 0) {
        return 0;
    }

    // "ERR" (command was not sucessful) is considered invalid, but we don't
    // need to check for it explicitly if it is not equal to "OK"

    // Check if the string's length matched the expected number of characters
    if (cmd_response_len != expected_len) {
        return 0;
    }

    // If none of the false conditions returned 0, the response is valid
    return 1;
}

/*
Waits until `cmd_response_available` is set to true by the UART RX callback,
then sets it back to false. Times out after 65,535 cycles if no response is available.
timeout_left - gets set by this function to the number of timeout cycles
               remaining when the response is available (starts at 65,535,
               counts down to 0); can be passed NULL to not receive the value
Returns - 1 for success (response available), 0 for failure (timed out)
*/
uint8_t wait_for_cmd_response(uint16_t *timeout_left) {
    uint16_t timeout = UINT16_MAX;
    while ((!cmd_response_available) && (timeout > 0)) {
        timeout--;
    }
    cmd_response_available = false;

    if (timeout_left != NULL) {
        *timeout_left = timeout;
    }

    if (timeout == 0) {
        return 0;
    } else {
        return 1;
    }
}




/*
Initializes the transceiver for UART RX callbacks (does not change any settings).
*/
void init_trans(void) {
    set_uart_rx_cb(trans_uart_rx_cb);
}

/*
UART RX callback
buf - array of received characters
len - number of received characters
Returns - number of characters processed

TODO - handle received messages from the ground station
*/
uint8_t trans_uart_rx_cb(const uint8_t* buf, uint8_t len) {
    // If we haven't found the termination, don't process anything yet
    if (buf[len - 1] != '\r') {
        return 0;
    }

    // If we found the termination:

    // Copy each character
    for (uint8_t i = 0; i < len - 1; ++i) {
        cmd_response[i] = buf[i];
    }
    // Add termination character
    cmd_response[len - 1] = '\0';
    // Set length
    cmd_response_len = len - 1;
    // Set command available flag
    cmd_response_available = true;

    print("received response: %u chars: %s\n", cmd_response_len, cmd_response);

    // Processed all characters
    return len;
}




uint8_t set_trans_scw_attempt(uint16_t scw) {
    // Send command
    print("\rES+W%02X00%04X\r", TRANS_ADDR, scw);

    // Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //Check validity
    uint8_t validity = valid_cmd_response(7);
    return validity;
}

/*
1. Write to status control register (p. 15-16)

scw - 16-bit data to write to register
Returns - 1 for success, 0 for failure
*/
uint8_t set_trans_scw(uint16_t scw) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = set_trans_scw_attempt(scw);
    }
    return ret;
}


uint8_t get_trans_scw_attempt(uint8_t* rssi, uint8_t* reset_count, uint16_t* scw) {
    // Send command
    print("\rES+R%02X00\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(13);

    // Extract values
    if (validity == 1) {
        if (rssi != NULL) {
            *rssi = (uint8_t) scan_uint(cmd_response, 3, 2);
        }
        if (reset_count != NULL) {
            *reset_count = (uint8_t) scan_uint(cmd_response, 7, 2);
        }
        if (scw != NULL) {
            *scw = (uint16_t) scan_uint(cmd_response, 9, 4);
        }
    }

    return validity;
}

/*
1. Read status control register (p. 15-16)

rssi - this function will set it to the last RSSI
reset_count - this function will set it to the number of times the transceiver
    has been reset
scw - this function will set it to the 16-bit register data of register
Returns - 1 for success, 0 for failure

Example (different format from datasheet):
ES+R2200
OK+0022DD0303
00 - unknown, probably RSSI
22 - device address
DD - device reset counter (observed to increase by 1 every time the transceiver is reset or power cycled)
0303 - contents of status register
*/
uint8_t get_trans_scw(uint8_t* rssi, uint8_t* reset_count, uint16_t* scw) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_scw_attempt(rssi, reset_count, scw);
    }
    return ret;
}


uint8_t set_trans_scw_bit_attempt(uint8_t bit_index, uint8_t value) {
    uint8_t ret;
    uint8_t rssi;
    uint8_t reset_count;
    uint16_t scw;

    ret = get_trans_scw(&rssi, &reset_count, &scw);
    if (ret == 0) {
        return 0;
    }

    if (value == 0) {
        scw &= ~_BV(bit_index);
    } else if (value == 1) {
        scw |= _BV(bit_index);
    }

    ret = set_trans_scw(scw);
    return ret;
}

/*
Sets the specified bit in the SCW register.
bit_index - index of the bit in the SCW register (MSB is 15, LSB is 0)
value - value to set the bit to (0 or 1)
Returns - 1 for success, 0 for failure
*/
uint8_t set_trans_scw_bit(uint8_t bit_index, uint8_t value) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = set_trans_scw_bit_attempt(bit_index, value);
    }
    return ret;
}


/*
1. Resets the transceiver (using status register)
Returns - 1 for success, 0 for failure

From testing, it seems that the transceiver responds with OK+8787 when the reset
bit is set. It seems that it preserves the value of the status register across
resets.
*/
uint8_t reset_trans(void) {
    uint8_t ret = set_trans_scw_bit(TRANS_RESET, 1);
    // Delay to give the transceiver time to actually reset
    // (without a delay, operations done quickly after reset will always fail)
    _delay_ms(5000);
    return ret;
}


uint8_t set_trans_rf_mode_attempt(uint8_t mode) {
    uint8_t ret;
    uint8_t rssi;
    uint8_t reset_count;
    uint16_t scw;

    ret = get_trans_scw(&rssi, &reset_count, &scw);
    if (ret == 0) {
        return 0;
    }

    // Clear and set bits 10-8
    scw &= 0xF8FF;
    scw |= (mode << 8);

    ret = set_trans_scw(scw);
    return ret;
}

/*
Sets the RF Mode in the SCW register (p. 15).
mode - must be between 0 and 7 (see p. 15 table)
Returns - 1 for success, 0 for failure
*/
uint8_t set_trans_rf_mode(uint8_t mode) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = set_trans_rf_mode_attempt(mode);
    }
    return ret;
}


/*
1. Turn on echo mode (using status register, p. 16)
Returns - 1 for success, 0 for failure
*/
uint8_t turn_on_trans_echo(void) {
    return set_trans_scw_bit(TRANS_ECHO, 1);
}


/*
1. Turn off echo mode (using status register, p. 16)
Returns - 1 for success, 0 for failure
*/
uint8_t turn_off_trans_echo(void) {
    return set_trans_scw_bit(TRANS_ECHO, 0);
}


/*
1. Turn on beacon mode (using status register)
Returns - 1 for success, 0 for failure
*/
uint8_t turn_on_trans_beacon(void) {
    return set_trans_scw_bit(TRANS_BCN, 1);
}


/*
1. Turn off beacon mode (using status register)
Returns - 1 for success, 0 for failure
*/
uint8_t turn_off_trans_beacon(void) {
    return set_trans_scw_bit(TRANS_BCN, 0);
}


/*
1. Turn on pipe (transparent) mode (using status control register) (p. 15-16).
The transceiver will turn off pipe mode on its own based on the pipe timeout.
Returns - 1 for success, 0 for failure
*/
uint8_t turn_on_trans_pipe(void) {
    return set_trans_scw_bit(TRANS_PIPE, 1);
}


uint8_t set_trans_freq_attempt(uint32_t freq) {
    print("\rES+W%02X01%08lX\r", TRANS_ADDR, freq);

    // Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //Check validity
    uint8_t validity = valid_cmd_response(2);
    return validity;
}

/*
2. Set transceiver frequency (p. 17-18)

freq - frequency to write, already in the converted 32-bit format that the
       transceiver expects (the output of the fwu conversion program)
Returns - 1 for success, 0 for failure
*/
uint8_t set_trans_freq(uint32_t freq) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = set_trans_freq_attempt(freq);
    }
    return ret;
}


uint8_t get_trans_freq_attempt(uint8_t* rssi, uint32_t* freq) {
    print("\rES+R%02X01\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(13);

    if (validity == 1) {
        if (rssi != NULL) {
            *rssi = (uint8_t) scan_uint(cmd_response, 3, 2);
        }
        if (freq != NULL) {
            *freq = scan_uint(cmd_response, 5, 8);
        }
    }

    return validity;
}

/*
2. Get transceiver frequency (p. 17-18)
rssi -  is set by this function to the last RSSI value
freq - will be set to the read 32-bit value
Returns - 1 for sucess, 0 for failure

Answer format: OK+[RR][FFFFFF][NN]
*/
uint8_t get_trans_freq(uint8_t* rssi, uint32_t* freq) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_freq_attempt(rssi, freq);
    }
    return ret;
}


uint8_t set_trans_pipe_timeout_attempt(uint8_t timeout) {
    print("\rES+W%02X06000000%02X\r", TRANS_ADDR, timeout);

    // Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //Check validity
    uint8_t validity = valid_cmd_response(2);
    return validity;
}

/*
4. Set transparent (pipe) mode timeout (p.18)
Sets timeout to turn off pipe mode if there are no UART messages
timeout - timeout (in seconds) to set
Returns - 1 for success, 0 for failure
*/
uint8_t set_trans_pipe_timeout(uint8_t timeout) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = set_trans_pipe_timeout_attempt(timeout);
    }
    return ret;
}


uint8_t get_trans_pipe_timeout_attempt(uint8_t* rssi, uint8_t* timeout) {
    print("\rES+R%02X06\r", TRANS_ADDR);

    // Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //Check validity
    uint8_t validity = valid_cmd_response(13);

    // Extract values
    if (validity == 1) {
        if (rssi != NULL) {
            *rssi = (uint8_t) scan_uint(cmd_response, 3, 2);
        }
        if (timeout != NULL) {
            *timeout = (uint8_t) scan_uint(cmd_response, 11, 2);
        }
    }

    return validity;
}

/*
4. Get transparent (pipe) mode timeout (p.18)
Gets timeout to turn off pipe mode if there are no UART messages
rssi -  is set by this function to the last RSSI value
timeout - is set by this function to the timeout (in seconds)
Returns - 1 for success, 0 for failure
*/
uint8_t get_trans_pipe_timeout(uint8_t* rssi, uint8_t* timeout) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_pipe_timeout_attempt(rssi, timeout);
    }
    return ret;
}


uint8_t set_trans_beacon_period_attempt(uint16_t period) {
    print("\rES+W%02X070000%04X\r", TRANS_ADDR, period);

    // Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //Check validity
    uint8_t validity = valid_cmd_response(2);
    return validity;
}

/*
5. Set beacon transmission period - Default is 60s (p. 19)
period - desired period between beacon message transmissions (in seconds)
Max val = 0xFFFF = 65535s = 1092min = 18.2h
Returns - 1 for success, 0 for failure
*/
uint8_t set_trans_beacon_period(uint16_t period) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = set_trans_beacon_period_attempt(period);
    }
    return ret;
}


uint8_t get_trans_beacon_period_attempt(uint8_t* rssi, uint16_t* period) {
    print("\rES+R%02X07\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(13);

    if (validity == 1) {
        if (rssi != NULL) {
            *rssi = (uint8_t) scan_uint(cmd_response, 3, 2);
        }
        if (period != NULL) {
            *period = (uint16_t) scan_uint(cmd_response, 9, 4);
        }
    }

    return validity;
}

/*
5. Get beacon transmission period (p. 19)
rssi -  is set by this function to the last RSSI value
period - set to desired period between beacon message transmissions (in seconds)
Max val = 0xFFFF = 65535s = 1092min = 18.2h
Returns - 1 for success, 0 for failure

Answer: OK+[RR]0000[TTTT]<CR>
*/
uint8_t get_trans_beacon_period(uint8_t* rssi, uint16_t* period) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_beacon_period_attempt(rssi, period);
    }
    return ret;
}


uint8_t set_trans_dest_call_sign_attempt(char* call_sign) {
    print("\rES+W%02XF5", TRANS_ADDR);
    for (uint8_t i = 0; i < TRANS_CALL_SIGN_LEN; i++) {
        print("%c", call_sign[i]);
    }
    print("\r");

    // Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //Check validity
    uint8_t validity = valid_cmd_response(2);
    return validity;
}

/*
7. Set destination call-sign (p.20)
call_sign - the call sign to set (6-byte array without termination or 7-byte array with termination)
Returns - 1 for success, 0 for failure
*/
uint8_t set_trans_dest_call_sign(char* call_sign) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = set_trans_dest_call_sign_attempt(call_sign);
    }
    return ret;
}


uint8_t get_trans_dest_call_sign_attempt(char* call_sign) {
    print("\rES+R%02XF5\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(9);

    if (validity == 1) {
        if (call_sign != NULL) {
            for (uint8_t i = 0; i < TRANS_CALL_SIGN_LEN; i++) {
                call_sign[i] = cmd_response[3 + i];
            }
            call_sign[TRANS_CALL_SIGN_LEN] = '\0';
        }
    }

    return validity;
}

/*
7. Get destination call-sign (p.20)
call_sign - a 7-char array that will be set by this function to the call sign read (6 chars plus '\0' termination)
Returns - 1 for success, 0 for failure

Answer: OK+DDDDDD<CR>
*/
uint8_t get_trans_dest_call_sign(char* call_sign) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_dest_call_sign_attempt(call_sign);
    }
    return ret;
}


uint8_t set_trans_src_call_sign_attempt(char* call_sign) {
    print("\rES+W%02XF6", TRANS_ADDR);
    for (uint8_t i = 0; i < TRANS_CALL_SIGN_LEN; i++) {
        print("%c", call_sign[i]);
    }
    print("\r");

    // Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //Check validity
    uint8_t validity = valid_cmd_response(2);
    return validity;
}

/*
8. Set source call-sign (p.20)
call_sign - the call sign to set (6-byte array without termination or 7-byte array with termination)
Returns - 1 for success, 0 for failure
*/
uint8_t set_trans_src_call_sign(char* call_sign) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = set_trans_src_call_sign_attempt(call_sign);
    }
    return ret;
}


uint8_t get_trans_src_call_sign_attempt(char* call_sign) {
    print("\rES+R%02XF6\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(9);

    if (validity == 1) {
        if (call_sign != NULL) {
            for (uint8_t i = 0; i < TRANS_CALL_SIGN_LEN; i++) {
                call_sign[i] = cmd_response[3 + i];
            }
            call_sign[TRANS_CALL_SIGN_LEN] = '\0';
        }
    }

    return validity;
}

/*
8. Get source call-sign (p.20)
call_sign - a 7-char array that will be set by this function to the call sign read (6 chars plus '\0' termination)
Returns - 1 for success, 0 for failure

Answer: OK+DDDDDD<CR>
*/
uint8_t get_trans_src_call_sign(char* call_sign) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_src_call_sign_attempt(call_sign);
    }
    return ret;
}


uint8_t get_trans_uptime_attempt(uint8_t* rssi, uint32_t* uptime) {
    print("\rES+R%02X02\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(13);

    if (validity == 1) {
        if (rssi != NULL) {
            *rssi = (uint8_t) scan_uint(cmd_response, 3, 2);
        }

        if (uptime != NULL) {
            *uptime = scan_uint(cmd_response, 5, 8);
        }
    }

    return validity;
}

/*
16. Get uptime (p. 23)
rssi - gets set to the last RSSI
uptime - gets set to the uptime
Returns - 1 for success, 0 for failure
*/
uint8_t get_trans_uptime(uint8_t* rssi, uint32_t* uptime) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_uptime_attempt(rssi, uptime);
    }
    return ret;
}


uint8_t get_trans_num_tx_packets_attempt(uint8_t* rssi, uint32_t* num_tx_packets) {
    print("\rES+R%02X03\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(13);

    if (validity == 1) {
        if (rssi != NULL) {
            *rssi = (uint8_t) scan_uint(cmd_response, 3, 2);
        }
        if (num_tx_packets != NULL) {
            *num_tx_packets = scan_uint(cmd_response, 5, 8);
        }
    }

    return validity;
}

/*
17. Get number of transmitted packets (p.23)
rssi - gets set to the last RSSI
num_tx_packets - gets set to the number of transmitted packets
Returns - 1 for success, 0 for failure
*/
uint8_t get_trans_num_tx_packets(uint8_t* rssi, uint32_t* num_tx_packets) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_num_tx_packets_attempt(rssi, num_tx_packets);
    }
    return ret;
}


uint8_t get_trans_num_rx_packets_attempt(uint8_t* rssi, uint32_t* num_rx_packets) {
    print("\rES+R%02X04\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(13);

    if (validity == 1) {
        if (rssi != NULL) {
            *rssi = (uint8_t) scan_uint(cmd_response, 3, 2);
        }
        if (num_rx_packets != NULL) {
            *num_rx_packets = scan_uint(cmd_response, 5, 8);
        }
    }

    return validity;
}

/*
18. Get number of received packets (p.23)
rssi - gets set to the last RSSI
num_rx_packets - gets set to the number of received packets
Returns - 1 for success, 0 for failure
*/
uint8_t get_trans_num_rx_packets(uint8_t* rssi, uint32_t* num_rx_packets) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_num_rx_packets_attempt(rssi, num_rx_packets);
    }
    return ret;
}


uint8_t get_trans_num_rx_packets_crc_attempt(uint8_t* rssi, uint32_t* num_rx_packets_crc) {
    print("\rES+R%02X05\r", TRANS_ADDR);

    //Wait for response
    uint8_t ret = wait_for_cmd_response(NULL);
    if (ret == 0) {
        return 0;
    }

    //check validity
    uint8_t validity = valid_cmd_response(13);

    if (validity == 1) {
        if (rssi != NULL) {
            *rssi = (uint8_t) scan_uint(cmd_response, 3, 2);
        }
        if (num_rx_packets_crc != NULL) {
            *num_rx_packets_crc = scan_uint(cmd_response, 5, 8);
        }
    }

    return validity;
}

/*
19. Get number of received packets with CRC error (p.23)
rssi - gets set to the last RSSI
num_rx_packets_crc - gets set to the number of received packets with CRC error
Returns - 1 for success, 0 for failure
*/
uint8_t get_trans_num_rx_packets_crc(uint8_t* rssi, uint32_t* num_rx_packets_crc) {
    uint8_t ret = 0;
    for (uint8_t i = 0; (i < TRANS_MAX_CMD_ATTEMPTS) && (ret == 0); i++) {
        ret = get_trans_num_rx_packets_crc_attempt(rssi, num_rx_packets_crc);
    }
    return ret;
}