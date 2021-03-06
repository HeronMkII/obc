/**
 * Harness tests for the command code on OBC. Tests that OBC responds and 
 * processes transceiver commands.
 * 
 * Not included in test as of yet:
 *  - The erase commands
 *  - Actuate pay motors
 *  - 
 *  - Reset subsystem
 *  - Resync data collection timers
 */

#include <test/test.h>
#include "../../src/command_utilities.h"
#include "../../src/commands.h"
#include "../../src/general.h"
#include "../../src/transceiver.h"

#define ASSERT_BYTES_EQ(bytes1, bytes2, count)  \
    for (uint8_t __i = 0; __i < (count); __i++) { \
        ASSERT_EQ((bytes1)[__i], (bytes2)[__i]);    \
    }

/**
 * Test some basic set and get commands
 */
void basic_commands_test (void) {
    // Set the date to yr = 255, month = 4, date = 1, disregard time
    enqueue_cmd(1, &set_rtc_cmd, 0x210401, 0); 
    enqueue_cmd(2, &get_rtc_cmd, 0, 0);
    
    execute_next_cmd();
    rtc_date_t date = read_rtc_date();
    rtc_time_t time = read_rtc_time();
    ASSERT_EQ(time.ss, 0);  // This could fail
    ASSERT_EQ(time.mm, 0);
    ASSERT_EQ(time.hh, 0);
    ASSERT_EQ(date.dd, 1);
    ASSERT_EQ(date.mm, 4);
    ASSERT_EQ(date.yy, 0x21);

    ASSERT_TRUE(trans_tx_dec_avail);
    
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 9);

        // This might fail
        ASSERT_EQ(trans_tx_dec_msg[3], date.yy);
        ASSERT_EQ(trans_tx_dec_msg[4], date.mm);
        ASSERT_EQ(trans_tx_dec_msg[5], date.dd);
        ASSERT_EQ(trans_tx_dec_msg[6], time.hh);
        ASSERT_EQ(trans_tx_dec_msg[7], time.mm);
        ASSERT_EQ(trans_tx_dec_msg[8], time.ss);
    }

    // Try to read 12 bytes of raw memory
    enqueue_cmd(4, &read_raw_mem_bytes_cmd, 0x200, 12);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail){
        ASSERT_EQ(trans_tx_dec_len, 15);
    }

    // Read the recent status info, just confirm length, content doesn't matter
    enqueue_cmd(12, &read_rec_status_info_cmd, 0, 0);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 36);    // OBC has 5 field --> 33 bytes in total + 3
    }

    // Trying to set a period < 60 should fail
    enqueue_cmd(13, &set_auto_data_col_period_cmd, 1, 40);
    execute_next_cmd();
    ASSERT_NEQ(obc_hk_data_col.auto_period, 40);
    ASSERT_TRUE(trans_tx_dec_avail);

    // Test get and set for data collection, data period
    enqueue_cmd(14, &set_auto_data_col_period_cmd, 1, 80);
    enqueue_cmd(15, &get_auto_data_col_settings_cmd, 0, 0);

    execute_next_cmd();
    ASSERT_EQ(obc_hk_data_col.auto_period, 80);
    ASSERT_TRUE(trans_tx_dec_avail);

    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 43);
        ASSERT_EQ(trans_tx_dec_msg[3 + 8], 80);
    }

    // Enable the data collection
    enqueue_cmd(16, &set_auto_data_col_enable_cmd, 1, 1);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    ASSERT_TRUE(obc_hk_data_col.auto_enabled);
    enqueue_cmd(17, &get_auto_data_col_settings_cmd, 0, 0);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 43);
        ASSERT_EQ(trans_tx_dec_msg[3 + 4], 1);
    }

    // Disable the data collection
    enqueue_cmd(19, &set_auto_data_col_enable_cmd, CMD_OBC_HK, 0);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    ASSERT_FALSE(obc_hk_data_col.auto_enabled);
    enqueue_cmd(20, &get_auto_data_col_settings_cmd, CMD_OBC_HK, 0);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 43);
        ASSERT_EQ(trans_tx_dec_msg[9], 0);
    }

    enqueue_cmd(25, &read_prim_cmd_blocks_cmd, 0, 5);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 3 + 5*21);
    }
}

/**
 *  Set the current block number for OBC, then read a data block.
 *  The current block number should increment after the read
 */
void data_collection_test(void) {
    int curr_block_num = 10;
    enqueue_cmd(55, &set_cur_block_num_cmd, CMD_OBC_HK, 10);
    enqueue_cmd(56, &get_cur_block_nums_cmd, CMD_OBC_HK, 0);
    
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 3 + (6 * 4));
        if (trans_tx_dec_len == 13) {
            ASSERT_EQ(trans_tx_dec_msg[9], 0);
            ASSERT_EQ(trans_tx_dec_msg[10], 0);
            ASSERT_EQ(trans_tx_dec_msg[11], 0);
            ASSERT_EQ(trans_tx_dec_msg[12], 10);
        }
    }

    // Collect a data block and make sure block number increments
    enqueue_cmd(57, &col_data_block_cmd, CMD_OBC_HK, 0);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 3 + 4);
    }
    ++curr_block_num;   // The block number should increase with a read

    enqueue_cmd(99, &get_cur_block_nums_cmd, CMD_OBC_HK, 0);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 3 + (6 * 4));
        ASSERT_EQ(trans_tx_dec_msg[3 + 3], curr_block_num);
    }
}

/**
 * Check memory commands:
 * - Setting and getting memory addresses
 * - Check corner cases such as setting invalid memory addresses (out of 
 *      range and end address < start address)
 */
void mem_commands_test(void) {
    int valid_start = 0x3e8;    // 1000
    int valid_end = 0x7d0;      // 2000
    // Set the memory addresses
    enqueue_cmd(5, &set_mem_sec_start_addr_cmd, 1, valid_start);
    enqueue_cmd(13, &set_mem_sec_end_addr_cmd, 1, valid_end);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    ASSERT_EQ(obc_hk_mem_section.start_addr, 0x3e8);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    ASSERT_EQ(obc_hk_mem_section.end_addr, 0x7d0);


    // Get the memory addresses
    enqueue_cmd(17, &get_mem_sec_addrs_cmd, CMD_OBC_HK, 0);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 3 + (6 * 8));
    }

    enqueue_cmd(20, &get_mem_sec_addrs_cmd, CMD_OBC_HK, 0);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    if (trans_tx_dec_avail) {
        ASSERT_EQ(trans_tx_dec_len, 3 + (6 * 4 * 2));
        ASSERT_EQ(trans_tx_dec_msg[3 + 0], 0);
        ASSERT_EQ(trans_tx_dec_msg[3 + 1], 0);
        ASSERT_EQ(trans_tx_dec_msg[3 + 2], 3);
        ASSERT_EQ(trans_tx_dec_msg[3 + 3], 0xe8);
        ASSERT_EQ(trans_tx_dec_msg[7 + 0], 0);
        ASSERT_EQ(trans_tx_dec_msg[7 + 1], 0);
        ASSERT_EQ(trans_tx_dec_msg[7 + 2], 7);
        ASSERT_EQ(trans_tx_dec_msg[7 + 3], 0xd0);
    }

    // Set the start address to an out of range address
    enqueue_cmd(42, &set_mem_sec_start_addr_cmd, 1, 0x600001);
    execute_next_cmd();
    ASSERT_TRUE(trans_tx_dec_avail);
    ASSERT_EQ(obc_hk_mem_section.start_addr, 0x3e8);
}

// Test that when an erase memory sector command is enqueued, it goes directly
// to the front of the queue
void auto_erase_mem_sector_test(void) {
    // Make sure queues are empty after any previous tests
    init_queue(&cmd_queue_1);
    init_queue(&cmd_queue_2);
    ASSERT_EQ(queue_size(&cmd_queue_1), 0);
    ASSERT_EQ(queue_size(&cmd_queue_2), 0);

    // These got changed in a previous test, set them back to defaults
    set_mem_section_start_addr(&obc_hk_mem_section, MEM_OBC_HK_START_ADDR);
    set_mem_section_end_addr(&obc_hk_mem_section, MEM_OBC_HK_END_ADDR);

    // Each OBC block is 5 fields (15 bytes) + header (10 bytes)
    // Total number of bytes in section is 0x100000
    // Say we want to cross the sector boundary at 0xF0000 -> can fit 39,321 complete blocks

    // This block number should not rollover, but the next one should
    set_mem_section_curr_block(&obc_hk_mem_section, 39319);

    // Make sure OBC_HK section parameters are what we expect
    ASSERT_EQ(obc_hk_mem_section.start_addr, MEM_OBC_HK_START_ADDR);
    ASSERT_EQ(obc_hk_mem_section.end_addr, MEM_OBC_HK_END_ADDR);
    ASSERT_EQ(obc_hk_mem_section.curr_block, 39319);
    ASSERT_EQ(obc_hk_mem_section.curr_block_eeprom_addr, MEM_OBC_HK_CURR_BLOCK_EEPROM_ADDR);
    ASSERT_EQ(obc_hk_mem_section.fields_per_block, CAN_OBC_HK_FIELD_COUNT);

    uint8_t cmd_101_1[8];
    uint8_t cmd_101_2[8];
    enqueue_cmd(0x101, &col_data_block_cmd, CMD_OBC_HK, 0);
    cmd_to_bytes(0x101, &col_data_block_cmd, CMD_OBC_HK, 0, cmd_101_1, cmd_101_2);

    uint8_t cmd_102_1[8];
    uint8_t cmd_102_2[8];
    enqueue_cmd(0x102, &col_data_block_cmd, CMD_OBC_HK, 0);
    cmd_to_bytes(0x102, &col_data_block_cmd, CMD_OBC_HK, 0, cmd_102_1, cmd_102_2);

    uint8_t erase_1[8];
    uint8_t erase_2[8];
    cmd_to_bytes(CMD_CMD_ID_AUTO_ENQUEUED, &erase_mem_phy_sector_cmd, 0xF0000, 0, erase_1, erase_2);

    uint8_t cmd_105_1[8];
    uint8_t cmd_105_2[8];
    enqueue_cmd(0x105, &ping_obc_cmd, 0, 0);
    cmd_to_bytes(0x105, &ping_obc_cmd, 0, 0, cmd_105_1, cmd_105_2);

    uint8_t cmd_109_1[8];
    uint8_t cmd_109_2[8];
    enqueue_cmd(0x109, &get_rtc_cmd, 0, 0);
    cmd_to_bytes(0x109, &get_rtc_cmd, 0, 0, cmd_109_1, cmd_109_2);
    
    ASSERT_EQ(queue_size(&cmd_queue_1), 4);
    ASSERT_EQ(queue_size(&cmd_queue_2), 4);
    ASSERT_BYTES_EQ(cmd_queue_1.content[0], cmd_101_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[0], cmd_101_2, 8);
    ASSERT_BYTES_EQ(cmd_queue_1.content[1], cmd_102_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[1], cmd_102_2, 8);
    ASSERT_BYTES_EQ(cmd_queue_1.content[2], cmd_105_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[2], cmd_105_2, 8);
    ASSERT_BYTES_EQ(cmd_queue_1.content[3], cmd_109_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[3], cmd_109_2, 8);

    execute_next_cmd();

    // Should not get an erase memory sector command
    ASSERT_EQ(queue_size(&cmd_queue_1), 3);
    ASSERT_EQ(queue_size(&cmd_queue_2), 3);
    ASSERT_BYTES_EQ(cmd_queue_1.content[1], cmd_102_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[1], cmd_102_2, 8);
    ASSERT_BYTES_EQ(cmd_queue_1.content[2], cmd_105_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[2], cmd_105_2, 8);
    ASSERT_BYTES_EQ(cmd_queue_1.content[3], cmd_109_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[3], cmd_109_2, 8);

    execute_next_cmd();

    // Expect an erase memory sector command at the front of the queue
    ASSERT_EQ(queue_size(&cmd_queue_1), 3);
    ASSERT_EQ(queue_size(&cmd_queue_2), 3);
    ASSERT_BYTES_EQ(cmd_queue_1.content[1], erase_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[1], erase_2, 8);
    ASSERT_BYTES_EQ(cmd_queue_1.content[2], cmd_105_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[2], cmd_105_2, 8);
    ASSERT_BYTES_EQ(cmd_queue_1.content[3], cmd_109_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[3], cmd_109_2, 8);

    uint8_t read_1[8];
    uint8_t read_2[8];
    peek_queue(&cmd_queue_1, read_1);
    peek_queue(&cmd_queue_2, read_2);
    ASSERT_BYTES_EQ(read_1, erase_1, 8);
    ASSERT_BYTES_EQ(read_2, erase_2, 8);

    ASSERT_EQ(queue_size(&cmd_queue_1), 3);
    ASSERT_EQ(queue_size(&cmd_queue_2), 3);

    // Execute the auto erase memory sector command
    execute_next_cmd();

    ASSERT_EQ(queue_size(&cmd_queue_1), 2);
    ASSERT_EQ(queue_size(&cmd_queue_2), 2);
    ASSERT_BYTES_EQ(cmd_queue_1.content[2], cmd_105_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[2], cmd_105_2, 8);
    ASSERT_BYTES_EQ(cmd_queue_1.content[3], cmd_109_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[3], cmd_109_2, 8);

    // Ping
    execute_next_cmd();

    ASSERT_EQ(queue_size(&cmd_queue_1), 1);
    ASSERT_EQ(queue_size(&cmd_queue_2), 1);
    ASSERT_BYTES_EQ(cmd_queue_1.content[3], cmd_109_1, 8);
    ASSERT_BYTES_EQ(cmd_queue_2.content[3], cmd_109_2, 8);

    // Get RTC
    execute_next_cmd();

    ASSERT_EQ(queue_size(&cmd_queue_1), 0);
    ASSERT_EQ(queue_size(&cmd_queue_2), 0);
}



test_t t1 = { .name = "basic commands test", .fn = basic_commands_test };
test_t t2 = { .name = "data collection test", .fn = data_collection_test };
test_t t3 = { .name = "memory commands test", .fn = mem_commands_test };
test_t t4 = { .name = "auto erase mem sector test", .fn = auto_erase_mem_sector_test };

test_t* suite[] = {&t1, &t2, &t3, &t4};

int main( void ) {
    init_obc_phase1_core();

    run_tests(suite, sizeof(suite) / sizeof(suite[0]));
    return 0;
}
