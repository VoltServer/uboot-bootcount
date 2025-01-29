/**
 * Access and reset u-boot's "bootcount" counter for the TI AM625 and AM62A SoCs
 * which is stored in the RTC_SCRATCH2_REG.
 *
 * see:
 * - AM625 TRM: https://www.ti.com/lit/pdf/spruiv7
 *   Section 12.7.3.3.4 Scratch Registers
 *   Section 14.8.7.3.12.1 RTC_RTC_SCRATCH0_N Register
 *
 * - AM62ax TRM: https://www.ti.com/lit/pdf/spruj16
 *   Section 12.8.3.3.4 Scratch Registers
 *   Section 14.9.7.3.12.1 RTC_RTC_SCRATCH0_N Register
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "./constants.h"
#include "./memory.h"
#include "./dt.h"
#include "./am62x.h"

// spruiv7.pdf Section 14.8.7.3.12 RTC_RTC_RTC_SCRATCH0_N Register
// spruj16.pdf Section 14.9.7.3.12 RTC_RTC_RTC_SCRATCH0_N Register
#define AM62_RTCSS                0x2B1F0000ul
#define AM62_REG_SIZE             4ul          // registers are 4 bytes/ 32bit
// Offset = 30h + (j * 4h); where j = 0h to 7h:
#define AM62_SCRATCH2_REG_OFFSET  (0x30ul + 2*AM62_REG_SIZE)

// spruiv7.pdf Section 14.8.7.3.19 RTC_RTC_RTC_KICK0 Registers
// spruj16.pdf Section 14.9.7.3.19 RTC_RTC_RTC_KICK0 Registers
#define AM62_KICK0R_REG_OFFSET 0x70ul
#define AM62_KICK1R_REG_OFFSET 0x74ul
#define AM62_KICK0_MAGIC       0x83e70b13ul
#define AM62_KICK1_MAGIC       0x95a4f1e0ul

#define AM62_MEM_OFFSET (AM62_RTCSS + AM62_SCRATCH2_REG_OFFSET)
// We need to map the RTCSS block from SCRATCH2 up to the end of KICK1R:
#define AM62_MEM_LEN    (AM62_KICK1R_REG_OFFSET + AM62_REG_SIZE - AM62_SCRATCH2_REG_OFFSET)

bool is_am62() {
    return is_compatible_soc("ti,am625") || is_compatible_soc("ti,am62a7");
}

int am62_read_bootcount(uint16_t* val) {

    uint32_t *scratch2_addr = memory_open(AM62_MEM_OFFSET, AM62_MEM_LEN);
    if ( scratch2_addr == (void *)E_DEVICE ) {
        return E_DEVICE;
    }

    uint32_t scratch2_val = memory_read(scratch2_addr);
    // low two bytes are the value, high two bytes are magic
    if ((scratch2_val & 0xffff0000) != (BOOTCOUNT_MAGIC & 0xffff0000)) {
        return E_BADMAGIC;
    }

    *val = (uint16_t)(scratch2_val & 0x0000ffff);
    return 0;
}


int am62_write_bootcount(uint16_t val) {
    // NOTE: These must be volatile.
    // See https://github.com/brgl/busybox/blob/master/miscutils/devmem.c
    volatile uint32_t *scratch2_addr =
        (volatile uint32_t *)memory_open(AM62_MEM_OFFSET, AM62_MEM_LEN);
    if ( scratch2_addr == (void *)E_DEVICE ) {
        return E_DEVICE;
    }

    volatile uint32_t *kick0r = scratch2_addr + (AM62_KICK0R_REG_OFFSET - AM62_SCRATCH2_REG_OFFSET) / 4;
    volatile uint32_t *kick1r = kick0r + 1; // (AM62_KICK1R_REG_OFFSET - AM62_SCRATCH2_REG_OFFSET) / 4;

    // Disable write protection, then write to SCRATCH2
    *kick0r = AM62_KICK0_MAGIC;
    *kick1r = AM62_KICK1_MAGIC;
    uint32_t scratch2_val = (BOOTCOUNT_MAGIC & 0xffff0000) | (val & 0xffff);
    memory_write(scratch2_addr, scratch2_val);

    // re-lock the write protection register
    *kick1r = 0;

    // read back to verify:
    uint16_t read_val = 0;
    am62_read_bootcount(&read_val);
    if ( read_val != val ) {
        return E_WRITE_FAILED;
    }

    return 0;
}
