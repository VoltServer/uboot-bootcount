/**
 * Memory access
 *
 * This file is part of the uboot-bootcount (https://github.com/VoltServer/uboot-bootcount).
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

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "constants.h"
#include "memory.h"

void *memory_open(off_t offset, size_t len) {
    size_t pagesize;
    off_t page_base, page_offset;
    int fd;
    uint8_t *mem;

    /*
     * Truncate offset to a multiple of the page size, or mmap will fail.
     * see: https://stackoverflow.com/a/12041352/213983
     */
    pagesize = sysconf(_SC_PAGE_SIZE);
    page_base = (offset / pagesize) * pagesize;
    page_offset = offset - page_base;

    fd = open("/dev/mem", O_SYNC | O_RDWR);
    if (fd < 0) {
        perror("open_memory(): open(\"/dev/mem\") failed");
        return (void *)E_DEVICE;
    }

    mem = mmap(NULL, page_offset + len, PROT_READ | PROT_WRITE, MAP_SHARED,
	       fd, page_base);
    close(fd);

    if (mem == MAP_FAILED) {
        perror("memory_open(): mmap() failed");
        return (void *)E_DEVICE;
    }

    return (mem + page_offset);
}
