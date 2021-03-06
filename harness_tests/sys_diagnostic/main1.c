#include <stdlib.h>
#include <test/test.h>
#include <uptime/uptime.h>

#include "../../src/general.h"


/* Sends a message to request data from all EPS Housekeeping fields */
void eps_hk_test(void){
    for (uint8_t field = 0; field < CAN_EPS_HK_FIELD_COUNT; field++){
        uint8_t msg[8] = {0x00};
        enqueue_tx_msg(&eps_tx_msg_queue, CAN_EPS_HK, field, 0);
        send_next_eps_tx_msg();
        /* Delay to give time to send and receive message */
        for (uint32_t i = 0; i < 1000 && queue_empty(&data_rx_msg_queue); i++) {
            _delay_ms(1);
        }
        ASSERT_FALSE(queue_empty(&data_rx_msg_queue));
        dequeue(&data_rx_msg_queue, msg);
        ASSERT_EQ(msg[0], CAN_EPS_HK);
        ASSERT_EQ(msg[1], field);
        ASSERT_EQ(msg[2], CAN_STATUS_OK);
        ASSERT_EQ(msg[3], 0x00);
        /* Assert that data portion of message is not all zero */
        ASSERT_NEQ(msg[4] | msg[5]| msg[6] | msg[7], 0x00);
    }
}


/* Sends a message to request data from all PAY Housekeeping fields and
    verifies that data is sent back when expected */
void pay_hk_test(void){
    for (uint8_t field = 0; field < CAN_PAY_HK_FIELD_COUNT; field++){
        uint8_t msg[8] = {0x00};
        enqueue_tx_msg(&pay_tx_msg_queue, CAN_PAY_HK, field, 0);
        send_next_pay_tx_msg();
        /* Delay to give time to send and receive message */
        for (uint32_t i = 0; i < 1000 && queue_empty(&data_rx_msg_queue); i++) {
            _delay_ms(1);
        }
        ASSERT_FALSE(queue_empty(&data_rx_msg_queue));
        dequeue(&data_rx_msg_queue, msg);
        ASSERT_EQ(msg[0], CAN_PAY_HK);
        ASSERT_EQ(msg[1], field);
        ASSERT_EQ(msg[2], CAN_STATUS_OK);
        ASSERT_EQ(msg[3], 0x00);
        /* Assert that data portion of message is not all zero */
        ASSERT_NEQ(msg[4] | msg[5]| msg[6] | msg[7], 0x00);
    }
}

/* Sends a message to request data from PAY Optical fields and verifies that
data is sent back when expected. Only send the first field (0) and last field
(63) because it take a long time */
void pay_opt_test(void){
    for (uint8_t field = 0;
        field < CAN_PAY_OPT_TOT_FIELD_COUNT;
        field += (CAN_PAY_OPT_TOT_FIELD_COUNT - 1)){

        uint8_t msg[8] = {0x00};
        enqueue_tx_msg(&pay_tx_msg_queue, CAN_PAY_OPT, field, 0);
        send_next_pay_tx_msg();
        /* Delay to give time to send and receive message */
        for (uint32_t i = 0; i < 15000 && queue_empty(&data_rx_msg_queue); i++) {
            _delay_ms(1);
        }
        ASSERT_FALSE(queue_empty(&data_rx_msg_queue));
        dequeue(&data_rx_msg_queue, msg);
        ASSERT_EQ(msg[0], CAN_PAY_OPT);
        ASSERT_EQ(msg[1], field);
        ASSERT_EQ(msg[2], CAN_STATUS_OK);
        ASSERT_EQ(msg[3], 0x00);
        /* Assert that data portion of message is not all zero */
        ASSERT_NEQ(msg[4] | msg[5]| msg[6] | msg[7], 0x00);
    }
}

/* Resets the PAY SSM and verifies that the reset counter increments correctly */
void pay_reset_test(void){
    uint32_t stale_num_restarts = 0x00;

    pay_hb_dev.send_req_flag = true;
    _delay_ms(100);
    run_hb();
    _delay_ms(100);
    stale_num_restarts = pay_hb_dev.restart_count;

    send_hb_reset((hb_dev_t*) &pay_hb_dev);

    _delay_ms(1000);
    pay_hb_dev.send_req_flag = true;
    _delay_ms(100);
    run_hb();
    _delay_ms(100);

    ASSERT_EQ(pay_hb_dev.restart_count, stale_num_restarts + 1);
    ASSERT_EQ(pay_hb_dev.restart_reason, UPTIME_RESTART_REASON_EXTRF);
}

/* Resets the EPS SSM and verifies that the reset counter increments correctly */
void eps_reset_test(void){
    uint32_t stale_num_restarts = 0x00;

    eps_hb_dev.send_req_flag = true;
    _delay_ms(100);
    run_hb();
    _delay_ms(100);
    stale_num_restarts = eps_hb_dev.restart_count;

    send_hb_reset((hb_dev_t*) &eps_hb_dev);

    _delay_ms(1000);
    eps_hb_dev.send_req_flag = true;
    _delay_ms(100);
    run_hb();
    _delay_ms(100);

    ASSERT_EQ(eps_hb_dev.restart_count, stale_num_restarts + 1);
    ASSERT_EQ(eps_hb_dev.restart_reason, UPTIME_RESTART_REASON_EXTRF);
}


test_t t1 = { .name = "EPS Housekeeping Test", .fn = eps_hk_test };
test_t t2 = { .name = "PAY Housekeeping Test", .fn = pay_hk_test };
test_t t3 = { .name = "PAY Optical Test", .fn = pay_opt_test };
test_t t4 = { .name = "PAY Reset Test", .fn = pay_reset_test };
test_t t5 = { .name = "EPS Reset Test", .fn = eps_reset_test };

test_t* suite[] = { &t1, &t2, &t3, &t4, &t5 };

int main(void) {
    WDT_OFF();
    init_obc_phase1_core();
    init_hb(HB_OBC);

    run_tests(suite, sizeof(suite) / sizeof(suite[0]));
    return 0;
}
