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
 * https://github.com/radii/devmem2
 * https://stackoverflow.com/questions/12040303/how-to-access-physical-addresses-from-user-space-in-linux?noredirect=1&lq=1
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
//#include <inttypes.h> // used for PRIx32 macro
#include <config.h>

#define RTCSS                0x44E3E000ul
#define SCRATCH2_REG_OFFSET  0x68ul
#define REG_SIZE             4ul          // in bytes, registers are 32bit

#define KICK0R_REG_OFFSET 0x6Cul          // see PDF section 20.3.5.23
#define KICK1R_REG_OFFSET 0x70ul
#define KICK0_MAGIC       0x83E70B13ul
#define KICK1_MAGIC       0x95A4F1E0ul

#define BOOTCOUNT_MAGIC   0xB001C041ul    // from u-boot include/common.h


int main(int argc, char *argv[]) {

    off_t offset = RTCSS + SCRATCH2_REG_OFFSET;
    // we can easily map SCRATCH2, KICK0R and KICK1R since they are adjacent:
    size_t len = KICK1R_REG_OFFSET + REG_SIZE - SCRATCH2_REG_OFFSET;

    // Truncate offset to a multiple of the page size, or mmap will fail.
    size_t pagesize = sysconf(_SC_PAGE_SIZE);
    off_t page_base = (offset / pagesize) * pagesize;
    off_t page_offset = offset - page_base;

    int fd = open("/dev/mem", O_SYNC | O_RDWR);
    unsigned char *mem = mmap(NULL, page_offset + len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_base);
    if (mem == MAP_FAILED) {
        perror("Can't map memory");
        return -1;
    }

    unsigned char *scratch2 = mem + page_offset;
    unsigned char *kick0r = scratch2 + KICK0R_REG_OFFSET - SCRATCH2_REG_OFFSET;
    unsigned char *kick1r = scratch2 + KICK1R_REG_OFFSET - SCRATCH2_REG_OFFSET;

    // Disable write protection, then write to SCRATCH2
    if (argc == 2 && strcmp(argv[1], "-r") == 0) {
      *(uint32_t *)kick0r = KICK0_MAGIC;
      *(uint32_t *)kick1r = KICK1_MAGIC;
      *(uint32_t *)scratch2 = (BOOTCOUNT_MAGIC & 0xffff0000);
    }

    // Read value from SCRATCH2, verify magic number
    else if (argc == 1) {
      uint32_t val = *(uint32_t *)scratch2;
      //printf("%08" PRIx32 "\n", val);

      // low two bytes are the value, high two bytes are magic
      if ((val & 0xffff0000) != (BOOTCOUNT_MAGIC & 0xffff0000)) {
        fprintf(stderr, "Error: BOOTCOUNT_MAGIC does not match\n");
        return -1;
      }

      printf("%d\n", (uint16_t)(val & 0x0000ffff));
    }
    else {
      fprintf(stderr, "Usage: %s [-r]\n\n"
                      "Read and reset the u-boot 'bootcount' register on TI am 33xx devices.\n"
                      "If invoked without any arguments, this prints the current 'bootcount'\n"
                      "value to stdout.\n\n"
                      "OPTIONS:\n\n"
                      "\t -r\tReset the bootcount to 0\n\n"
                      "Package details:\t\t" PACKAGE_STRING "\n"
                      "Bug Reports:\t\t" PACKAGE_BUGREPORT "\n"
                      "Homepage:\t\t" PACKAGE_URL "\n\n", argv[0]);
      return 1;
    }

    // PS: we don't need to munmap(): it is freed when the process terminates
    return 0;
}
