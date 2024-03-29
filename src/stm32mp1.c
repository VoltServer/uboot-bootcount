/**
 * Access and reset u-boot's "bootcount" counter for the STM32MP1 platform
 * which is stored in the TAMP_BKP21R.
 *
 * See:
 * https://wiki.st.com/stm32mpu/wiki/STM32MP15_backup_registers#Boot_counter_feature
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
#include "./stm32mp1.h"

// See https://wiki.st.com/stm32mpu/wiki/STM32MP15_backup_registers#BOOT_COUNTER
#define TAMP_BKP0R 0x5C00A100ul
#define TAMP_BKP21R_OFFSET 0x54ul
#define REG_SIZE             4ul          // registers are 4 bytes/ 32bit

#define STM32MP1_MEM_OFFSET (TAMP_BKP0R + TAMP_BKP21R_OFFSET)
#define STM32MP1_MEM_LEN (REG_SIZE)

bool is_stm32mp1() {
    return is_compatible_soc("st,stm32mp153") || is_compatible_soc("st,stm32mp157");
}

int stm32mp1_read_bootcount(uint16_t* val) {

    uint32_t *bkp21r = memory_open(STM32MP1_MEM_OFFSET, STM32MP1_MEM_LEN);
    if ( bkp21r == (void *)E_DEVICE ) {
        return E_DEVICE;
    }

    //printf("%08" PRIx32 "\n", *bkp21r);

    // low two bytes are the value, high two bytes are magic
    if ((*bkp21r & 0xffff0000) != (BOOTCOUNT_MAGIC & 0xffff0000)) {
        return E_BADMAGIC;
    }

    *val = (uint16_t)(*bkp21r & 0x0000ffff);
    return 0;
}


int stm32mp1_write_bootcount(uint16_t val) {
    // NOTE: These must be volatile.
    // See https://github.com/brgl/busybox/blob/master/miscutils/devmem.c
    volatile uint32_t *bkp21r =
        (volatile uint32_t *)memory_open(STM32MP1_MEM_OFFSET, STM32MP1_MEM_LEN);
    if ( bkp21r == (void *)E_DEVICE ) {
        return E_DEVICE;
    }

    *bkp21r = (BOOTCOUNT_MAGIC & 0xffff0000) | (val & 0xffff);

    // read back to verify:
    uint16_t read_val = 0;
    stm32mp1_read_bootcount(&read_val);
    if ( read_val != val ) {
        return E_WRITE_FAILED;
    }

    return 0;
}
