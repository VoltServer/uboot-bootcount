/**
 * Access and reset u-boot's "bootcount" counter for the TI AM33xx platform
 * which is stored in the RTC_SCRATCH2_REG.
 *
 * spruh73p.pdf Section 2.1 Memory Map, page 180
 * RTCSS 0x44E3_E000 - 0x44E3_EFFF 4KB RTC Registers
 * 20.3.5.22: RTC_SCRATCH2_REG Register (offset = 68h)
 * 20.3.5.23: KICK0R Register (offset = 6Ch)
 * 20.3.5.24: KICK1R Register (offset = 70h)
 *
 * See:
 * https://www.ti.com/lit/ug/spruh73p/spruh73p.pdf
 * http://www.denx.de/wiki/view/DULG/UBootBootCountLimit
 * http://git.ti.com/ti-u-boot/ti-u-boot/blobs/master/drivers/bootcount/bootcount_davinci.c
 * http://git.ti.com/ti-u-boot/ti-u-boot/blobs/master/arch/arm/include/asm/davinci_rtc.h
 *
 * This file is part of the uboot-bootcount (https://github.com/VoltServer/uboot-bootcount).
 * Copyright (c) 2018 VoltServer.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "./constants.h"
#include "./memory.h"
#include "./dt.h"
#include "./am33xx.h"

// See u-boot arch/arm/include/asm/davinci_rtc.h:
#define RTCSS                0x44E3E000ul
#define SCRATCH2_REG_OFFSET  0x68ul
#define REG_SIZE             4ul          // registers are 4 bytes/ 32bit

#define KICK0R_REG_OFFSET 0x6Cul          // see PDF section 20.3.5.23
#define KICK1R_REG_OFFSET 0x70ul
#define KICK0_MAGIC       0x83E70B13ul
#define KICK1_MAGIC       0x95A4F1E0ul

#define AM33XX_MEM_OFFSET (RTCSS + SCRATCH2_REG_OFFSET)
#define AM33XX_MEM_LEN    (KICK1R_REG_OFFSET + REG_SIZE - SCRATCH2_REG_OFFSET)

bool is_am33() {
    return is_compatible_soc("ti,am33xx");
}

int am33_read_bootcount(uint16_t* val) {

    uint32_t *scratch2_addr = memory_open(AM33XX_MEM_OFFSET, AM33XX_MEM_LEN);
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


int am33_write_bootcount(uint16_t val) {
    // NOTE: These must be volatile.
    // See https://github.com/brgl/busybox/blob/master/miscutils/devmem.c
    volatile uint32_t *scratch2_addr =
        (volatile uint32_t *)memory_open(AM33XX_MEM_OFFSET, AM33XX_MEM_LEN);
    if ( scratch2_addr == (void *)E_DEVICE ) {
        return E_DEVICE;
    }

    volatile uint32_t *kick0r = scratch2_addr + 1;    // next 32-bit register after SCRATCH2
    volatile uint32_t *kick1r = kick0r + 1;      // next 32-bit register after KICK0R

    // Disable write protection, then write to SCRATCH2
    *kick0r = KICK0_MAGIC;
    *kick1r = KICK1_MAGIC;
    uint32_t scratch2_val = (BOOTCOUNT_MAGIC & 0xffff0000) | (val & 0xffff);
    memory_write(scratch2_addr, scratch2_val);

    // read back to verify:
    uint16_t read_val = 0;
    am33_read_bootcount(&read_val);
    if ( read_val != val ) {
        return E_WRITE_FAILED;
    }

    return 0;
}
