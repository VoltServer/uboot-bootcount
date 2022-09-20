/**
 * Access and reset u-boot's "bootcount" counter for the STM32MP1 platform
 * which is stored in the TAMP_BKP21R.
 *
 * This file is part of the uboot-davinci-bootcount (https://github.com/VoltServer/uboot-davinci-bootcount).
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
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "./constants.h"
#include "./stm32mp1.h"

void *_stm32_open_memory();

int stm32mp1_read_bootcount(uint16_t* val) {

    uint32_t *bkp21r = _stm32_open_memory();
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
    volatile uint32_t *bkp21r = (volatile uint32_t *)_stm32_open_memory();
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

void *_stm32_open_memory() {
    off_t offset = TAMP_BKP0R + TAMP_BKP21R_OFFSET;
    // Only need to map a single register:
    size_t len = REG_SIZE;

    // Truncate offset to a multiple of the page size, or mmap will fail.
    // see: https://stackoverflow.com/a/12041352/213983
    size_t pagesize = sysconf(_SC_PAGE_SIZE);
    off_t page_base = (offset / pagesize) * pagesize;
    off_t page_offset = offset - page_base;

    int fd = open("/dev/mem", O_SYNC | O_RDWR);

    if ( fd < 0 ) {
        perror("open_memory(): open(\"/dev/mem\") failed");
        return (void *)E_DEVICE;
    }

    uint8_t *mem = mmap(NULL, page_offset + len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_base);
    close(fd);

    if (mem == MAP_FAILED) {
        perror("open_memory(): mmap() failed");
        return (void *)E_DEVICE;
    }

    return (mem + page_offset); // This is a pointer to TAMP_BKP21R register
}


