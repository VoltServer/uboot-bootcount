/**
 * Access and reset u-boot's "bootcount" counter for IMX93 platform
 *
 * This file is part of the uboot-bootcount (https://github.com/VoltServer/uboot-bootcount).
 *
 * See:
 * - IMX93RM.pdf: i.MX 93 Applications Processor Reference Manual
 *
 * Chapter 33 Battery-Backed Non-Secure Module (BBNSM)
 * Section 33.6.1.1 BBNSM memory map
 * Section 33.6.1.11 General Purpose Register Word a (GPR0 - GPR7)
 *
 * Copyright (c) 2024 ELCO Elettronica Automation s.r.l., Stefano Costa <s.costa@elcoelettronica.it>
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
#include <unistd.h>

#include "constants.h"
#include "dt.h"
#include "memory.h"

#define BBNSM_BASE_ADDR 0x44440000
#define BBNSM_GPR0_ALIAS_REG_OFFSET 0x300
#define BBNSM_GPR0_ALIAS_REG_SIZE 4

#define IMX93_MEM_OFFSET (BBNSM_BASE_ADDR + BBNSM_GPR0_ALIAS_REG_OFFSET)
#define IMX93_MEM_LEN (BBNSM_GPR0_ALIAS_REG_SIZE)

bool is_imx93() {
    return is_compatible_soc("fsl,imx93");
}

int imx93_read_bootcount(uint16_t* val) {

    uint32_t *gpr0;

    gpr0 = memory_open(IMX93_MEM_OFFSET, IMX93_MEM_LEN);
    if (gpr0 == (void *)E_DEVICE)
        return E_DEVICE;

    /* low two bytes are the value, high two bytes are magic */
    if ((*gpr0 & 0xffff0000) != (BOOTCOUNT_MAGIC & 0xffff0000))
        return E_BADMAGIC;

    *val = (uint16_t)(*gpr0 & 0x0000ffff);
    return 0;
}

int imx93_write_bootcount(uint16_t val) {
    // NOTE: These must be volatile.
    // See https://github.com/brgl/busybox/blob/master/miscutils/devmem.c
    volatile uint32_t *gpr0;
    uint16_t read_val = 0;

    gpr0 = (volatile uint32_t *)memory_open(IMX93_MEM_OFFSET, IMX93_MEM_LEN);
    if (gpr0 == (void *)E_DEVICE )
        return E_DEVICE;

    *gpr0 = (BOOTCOUNT_MAGIC & 0xffff0000) | (val & 0xffff);

    /* read back to verify */
    imx93_read_bootcount(&read_val);
    if (read_val != val)
        return E_WRITE_FAILED;

    return 0;
}
