
/**
 * i2c-EEPROM boot counter.
 * Ref: https://github.com/u-boot/u-boot/blob/master/drivers/bootcount/i2c-eeprom.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "constants.h"
#include "i2c_eeprom.h"

int open_eeprom(uint8_t bus, uint8_t addr, off_t offset);

int eeprom_read_bootcount2(uint16_t *val, uint8_t bus, uint8_t addr, off_t offset);

int eeprom_write_bootcount2(uint16_t val, uint8_t bus, uint8_t addr, off_t offset);

int eeprom_read_bootcount(uint16_t *val) {
    return eeprom_read_bootcount2(val, DEFAULT_I2C_BUS, DEFFAULT_I2C_ADDR, DEFAULT_OFFSET);
}

int eeprom_write_bootcount(uint16_t val) {
    return eeprom_write_bootcount2(val, DEFAULT_I2C_BUS, DEFFAULT_I2C_ADDR, DEFAULT_OFFSET);
}

// https://github.com/u-boot/u-boot/blob/master/drivers/bootcount/i2c-eeprom.c#L34
int eeprom_read_bootcount2(uint16_t *val, uint8_t bus, uint8_t addr, off_t offset) {

    uint16_t data;
    int fd = open_eeprom(bus, addr, offset);
    if ( fd == E_DEVICE ) {
        return E_DEVICE;
    }
    if ( read(fd, &data, sizeof(data)) == -1 ) {
        perror("Read error");
        return E_DEVICE;
    }

    if (data >> 8 != EEPROM_MAGIC) {
        return E_BADMAGIC;
    }

    *val = data & 0xff;
    return 0;
}

// https://github.com/u-boot/u-boot/blob/master/drivers/bootcount/i2c-eeprom.c#L20
int eeprom_write_bootcount2(uint16_t val, uint8_t bus, uint8_t addr, off_t offset) {

    int fd = open_eeprom(bus, addr, offset);
    if ( fd == E_DEVICE ) {
        return E_DEVICE;
    }

    uint16_t data = 0;
    data = EEPROM_MAGIC << 8 | (val & 0xff);

    ssize_t written = write(fd, &data, sizeof(data));
    if ( written == -1 ) {
        perror("Write error");
        return E_DEVICE;
    }
    if ( (size_t)written < sizeof(data) ) {
        fprintf(stderr, "Incomplete write: %d bytes!\n", written);
    }
    close(fd);

    return 0;
}


int open_eeprom(uint8_t bus, uint8_t addr, off_t offset) {
    char eeprom_path[40];
    int fd;
    struct stat sb;

    snprintf(eeprom_path, sizeof(eeprom_path), EEPROM_PATH, bus, addr);

    fd = open(eeprom_path, O_RDWR);

    if (fd < 0) {
        perror("open failed");
        return E_DEVICE;
    }

    if (fstat(fd, &sb) == -1) {
        perror("fstat failed");
        return E_DEVICE;
    }
    //printf("Opened path %s\n", eeprom_path);

    if( lseek(fd, offset, SEEK_SET) == -1 ) {
        perror("Seek failed");
        return E_DEVICE;
    }

    return fd;
}

bool eeprom_exists(void) {
    char eeprom_path[40];
    snprintf(eeprom_path, sizeof(eeprom_path), EEPROM_PATH, DEFAULT_I2C_BUS, DEFFAULT_I2C_ADDR);

    struct stat sb;
    if ( stat(eeprom_path, &sb) == -1 ) {
        return false;
    }
    return true;
}
