#ifndef _can_handler_h
#define _can_handler_h

#include <avr/io.h>
#include <util/delay.h>
#include <can/can.h>

#include "data_buffer.h"

struct data_buffer * data_buffer_pointer;

void can_handler ( uint16_t identifier, uint8_t* pt_data, uint8_t size);

#endif
