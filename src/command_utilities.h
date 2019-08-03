#ifndef COMMAND_UTILITIES_H
#define COMMAND_UTILITIES_H

#include <stdbool.h>

#include <queue/queue.h>

#include "mem.h"
#include "transceiver.h"


// Callback function signature to run a command
typedef void(*cmd_fn_t)(void);

typedef struct {
    cmd_fn_t fn;
    uint8_t num;
} cmd_t;

// Need to declare `cmd_t` before this include to prevent errors from ordering
// of header includes
#include "commands.h"


// Subsystem
#define TRANS_CMD_OBC       0
#define TRANS_CMD_EPS       1
#define TRANS_CMD_PAY       2

// Block types
#define TRANS_CMD_EPS_HK    0
#define TRANS_CMD_PAY_HK    1
#define TRANS_CMD_PAY_OPT   2

// Command types
#define TRANS_CMD_PING                      0x00
#define TRANS_CMD_GET_SUBSYS_STATUS         0x01
#define TRANS_CMD_GET_RTC                   0x02
#define TRANS_CMD_SET_RTC                   0x03
#define TRANS_CMD_READ_MEM_BYTES            0x04
#define TRANS_CMD_ERASE_MEM_PHY_SECTOR      0x05
#define TRANS_CMD_COL_BLOCK                 0x06
#define TRANS_CMD_READ_LOC_BLOCK            0x07
#define TRANS_CMD_READ_MEM_BLOCK            0x08
#define TRANS_CMD_AUTO_DATA_COL_ENABLE      0x09
#define TRANS_CMD_AUTO_DATA_COL_PERIOD      0x0A
#define TRANS_CMD_AUTO_DATA_COL_RESYNC      0x0B
#define TRANS_CMD_PAY_ACT_MOTORS            0x0E
#define TRANS_CMD_RESET_SUBSYS              0x0F
#define TRANS_CMD_EPS_CAN                   0x10
#define TRANS_CMD_PAY_CAN                   0x11
#define TRANS_CMD_READ_EEPROM               0x12
#define TRANS_CMD_GET_CUR_BLOCK_NUM         0x13
#define TRANS_CMD_SET_CUR_BLOCK_NUM         0x14
#define TRANS_CMD_SET_MEM_SEC_START_ADDR    0x15
#define TRANS_CMD_SET_MEM_SEC_END_ADDR      0x16
#define TRANS_CMD_ERASE_EEPROM              0x17
#define TRANS_CMD_ERASE_ALL_MEM             0x19
#define TRANS_CMD_ERASE_MEM_PHY_BLOCK       0x1A

// Subsystems
#define CMD_SUBSYS_OBC  0
#define CMD_SUBSYS_EPS  1
#define CMD_SUBSYS_PAY  2

// Block types
#define CMD_BLOCK_EPS_HK    0
#define CMD_BLOCK_PAY_HK    1
#define CMD_BLOCK_PAY_OPT   2

// Max memory read
#define CMD_READ_MEM_MAX_COUNT (TRANS_TX_DEC_MSG_MAX_SIZE - 9)

// Default period for automatic data collection for each block type
// (time between timer callbacks, in seconds)
#define EPS_HK_AUTO_DATA_COL_PERIOD     60
#define PAY_HK_AUTO_DATA_COL_PERIOD     120
#define PAY_OPT_AUTO_DATA_COL_PERIOD    300


// Automatic data collection for one block type
typedef struct {
    // True if we are currently collecting this type of data
    bool enabled;
    // Seconds between collection
    uint32_t period;
    // Number of seconds counted (start at 0, go to `period`)
    uint32_t count;
} auto_data_col_t;


extern queue_t cmd_queue;
extern queue_t cmd_args_queue;

extern volatile cmd_t* volatile current_cmd;
extern volatile uint32_t current_cmd_arg1;
extern volatile uint32_t current_cmd_arg2;
extern volatile bool prev_cmd_succeeded;

extern volatile uint8_t can_countdown;

extern mem_header_t eps_hk_header;
extern uint32_t eps_hk_fields[];
extern mem_header_t pay_hk_header;
extern uint32_t pay_hk_fields[];
extern mem_header_t pay_opt_header;
extern uint32_t pay_opt_fields[];

extern volatile auto_data_col_t eps_hk_auto_data_col;
extern volatile auto_data_col_t pay_hk_auto_data_col;
extern volatile auto_data_col_t pay_opt_auto_data_col;

extern rtc_date_t restart_date;
extern rtc_time_t restart_time;


void handle_trans_rx_dec_msg(void);

void start_trans_tx_dec_msg(void);
void append_to_trans_tx_dec_msg(uint8_t byte);
void finish_trans_tx_dec_msg(void);

cmd_t* trans_msg_type_to_cmd(uint8_t msg_type);
uint8_t trans_cmd_to_msg_type(cmd_t* cmd);

void enqueue_cmd(cmd_t* cmd, uint32_t arg1, uint32_t arg2);
void dequeue_cmd(void);

void execute_next_cmd(void);
void finish_current_cmd(bool succeeded);

void populate_header(mem_header_t* header, uint32_t block_num, uint8_t error);

void append_header_to_tx_msg(mem_header_t* header);
void append_fields_to_tx_msg(uint32_t* fields, uint8_t num_fields);

void auto_data_col_timer_cb(void);
void can_timer_cb(void);

#endif