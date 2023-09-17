/**
 * Access and reset u-boot's "bootcount" counter for IMX8M platform
 *
 * This file is part of the uboot-bootcount (https://github.com/VoltServer/uboot-bootcount).
 *
 * See:
 * - IMX8MPRM.pdf: i.MX 8M Plus Applications Processor Reference Manual
 * - IMX8MNRM.pdf: i.MX 8M Nano Applications Processor Reference Manual
 * - IMX8MDQLQRM.pdf: i.MX 8M Dual/8M QuadLite/8M Quad Applications Processors Reference Manual
 *
 * Section 6.4 Secure Non-Volatile Storage (SNVS)
 * Section 6.4.5.1 SNVS Memory map
 * Section 6.4.5.16 SNVS_LP General Purpose Registers 0 .. 3 (LPGPR0_alias -LPGPR3_alias)
 *
 * Copyright (c) 2023 Amarula Solutions, Dario Binacchi <dario.binacchi@amarulasolutions.com>
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

#define SNVS_BASE_ADDR 0x30370000
#define SNVS_LPGPR0_ALIAS_REG_OFFSET 0x90
#define SNVS_LPGPR0_ALIAS_REG_SIZE 4

#define IMX8M_MEM_OFFSET (SNVS_BASE_ADDR + SNVS_LPGPR0_ALIAS_REG_OFFSET)
#define IMX8M_MEM_LEN (SNVS_LPGPR0_ALIAS_REG_SIZE)

bool is_imx8m() {
    return is_compatible_soc("fsl,imx8mm") || is_compatible_soc("fsl,imx8mn") ||
           is_compatible_soc("fsl,imx8mp") || is_compatible_soc("fsl,imx8mq");
}

int imx8m_read_bootcount(uint16_t* val) {

    uint32_t *gprg0;

    gprg0 = memory_open(IMX8M_MEM_OFFSET, IMX8M_MEM_LEN);
    if (gprg0 == (void *)E_DEVICE)
        return E_DEVICE;

    /* low two bytes are the value, high two bytes are magic */
    if ((*gprg0 & 0xffff0000) != (BOOTCOUNT_MAGIC & 0xffff0000))
        return E_BADMAGIC;

    *val = (uint16_t)(*gprg0 & 0x0000ffff);
    return 0;
}

int imx8m_write_bootcount(uint16_t val) {
    // NOTE: These must be volatile.
    // See https://github.com/brgl/busybox/blob/master/miscutils/devmem.c
    volatile uint32_t *gprg0;
    uint16_t read_val = 0;

    gprg0 = (volatile uint32_t *)memory_open(IMX8M_MEM_OFFSET, IMX8M_MEM_LEN);
    if (gprg0 == (void *)E_DEVICE )
        return E_DEVICE;

    *gprg0 = (BOOTCOUNT_MAGIC & 0xffff0000) | (val & 0xffff);

    /* read back to verify */
    imx8m_read_bootcount(&read_val);
    if (read_val != val)
        return E_WRITE_FAILED;

    return 0;
}
