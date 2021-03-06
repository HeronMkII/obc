#include "can_commands.h"

// Uncomment for extra debugging prints
// #define CAN_COMMANDS_DEBUG

queue_t eps_tx_msg_queue;
queue_t pay_tx_msg_queue;
queue_t data_rx_msg_queue;


// Set to true to print TX and RX CAN messages
bool print_can_msgs = false;


// If there is an RX messsage in the queue, process it
void process_next_rx_msg(void) {
    uint8_t msg[8] = {0x00};

    uint8_t opcode = 0;
    uint8_t status = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (queue_empty(&data_rx_msg_queue)) {
            return;
        }
        // Remove it from the queue, but we might put it back after
        dequeue(&data_rx_msg_queue, msg);
    }
    
    if (print_can_msgs) {
        // Extra spaces to align with CAN TX messages
        print("CAN RX:       ");
        print_bytes(msg, 8);
    }

    // Break down the message into components
    opcode = msg[0];
    status = msg[2];

    // If we are in the middle of a collect data block command for this type,
    // don't remove it from the queue
    if ((opcode == CAN_EPS_HK && cmd_queue_contains_col_data_block(CMD_EPS_HK)) ||
            (opcode == CAN_PAY_HK && cmd_queue_contains_col_data_block(CMD_PAY_HK)) ||
            (opcode == CAN_PAY_OPT && cmd_queue_contains_col_data_block(CMD_PAY_OPT))) {
        enqueue_front(&data_rx_msg_queue, msg);
        if (print_can_msgs) {
            print("Re-enqueued\n");
        }
        return;
    }

    // Continue with processing this message here

    //General CAN message command-Intercept and send back data
    // Use the status received in the CAN message as the command status
    if ((current_cmd == &send_eps_can_msg_cmd) || (current_cmd == &send_pay_can_msg_cmd)) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            start_trans_tx_resp(status);
            for (uint8_t i = 0; i < 8; i++) {
                append_to_trans_tx_resp(msg[i]);
            }
            finish_trans_tx_resp();
        }
        finish_current_cmd(status);
    }
}




/*
If there is a TX message in the EPS queue, sends it

When resume_mob(mob name) is called, it:
1) resumes the MOB
2) triggers an interrupt (callback function) to get the data to transmit
3) sends the data
4) pauses the mob
*/
void send_next_eps_tx_msg(void) {
    if (queue_empty(&eps_tx_msg_queue)) {
        return;
    }

    if (print_can_msgs) {
        uint8_t tx_msg[8] = { 0x00 };
        peek_queue(&eps_tx_msg_queue, tx_msg);
        print("CAN TX (EPS): ");
        print_bytes(tx_msg, 8);
    }

    resume_mob(&eps_cmd_tx_mob);
}

/*
If there is a TX message in the PAY queue, sends it

When resume_mob(mob name) is called, it:
1) resumes the MOB
2) triggers an interrupt (callback function) to get the data to transmit
3) sends the data
4) pauses the mob
*/
void send_next_pay_tx_msg(void) {
    if (queue_empty(&pay_tx_msg_queue)) {
        return;
    }

    if (print_can_msgs) {
        uint8_t tx_msg[8] = { 0x00 };
        peek_queue(&pay_tx_msg_queue, tx_msg);
        print("CAN TX (PAY): ");
        print_bytes(tx_msg, 8);
    }

    resume_mob(&pay_cmd_tx_mob);
}




// Enqueues a CAN message given a general set of 8 bytes data
void enqueue_tx_msg_bytes(queue_t* queue, uint32_t data1, uint32_t data2) {
    uint8_t msg[8] = { 0x00 };
    msg[0] = (data1 >> 24) & 0xFF;
    msg[1] = (data1 >> 16) & 0xFF;
    msg[2] = (data1 >> 8) & 0xFF;
    msg[3] = data1 & 0xFF;
    msg[4] = (data2 >> 24) & 0xFF;
    msg[5] = (data2 >> 16) & 0xFF;
    msg[6] = (data2 >> 8) & 0xFF;
    msg[7] = data2 & 0xFF;

    enqueue(queue, msg);
}

/*
Enqueues a CAN message onto the specified queue to request the specified message
    type and field number.
queue - Queue to enqueue the message to
opcode - Message type to request (byte 0)
field_num - Field number to request (byte 1)
*/
void enqueue_tx_msg(queue_t* queue, uint8_t opcode, uint8_t field_num, uint32_t data) {
    uint8_t msg[8] = { 0x00 };
    msg[0] = opcode;
    msg[1] = field_num;
    msg[2] = 0x00;  // OBC doesn't send status
    msg[3] = 0x00;
    msg[4] = (data >> 24) & 0xFF;
    msg[5] = (data >> 16) & 0xFF;
    msg[6] = (data >> 8) & 0xFF;
    msg[7] = data & 0xFF;

    enqueue(queue, msg);
}
