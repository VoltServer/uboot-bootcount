
/**
 * dm-eeprom bootcount implementation for linux userspace.
 * Ref: https://github.com/u-boot/u-boot/blob/master/drivers/bootcount/bootcount_dm_i2c.c
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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include <dirent.h>
#include <sys/types.h>

#include "dt.h"
#include "constants.h"

#define DM_I2C_MAGIC 0xbc

/*
 * Runtime discovery of the EEPROM used for bootcount via the flattened
 * device tree exported at /proc/device-tree.
 *
 * The definition looks like this:
 *
 *    chosen {
 *        // see: u-boot/drivers/bootcount/bootcount-uclass.c
 *        u-boot,bootcount-device = &bootcount_i2c_eeprom;
 *    };
 *    bootcount_i2c_eeprom: bc_i2c_eeprom {
 *        // see: u-boot/drivers/bootcount/i2c-eeprom.c
 *        compatible = "u-boot,bootcount-i2c-eeprom";
 *        i2c-eeprom = <&eeprom0>;
 *        offset = <0x30>;
 *    };
 *
 * see: https://github.com/u-boot/u-boot/blob/master/drivers/bootcount/i2c-eeprom.c
 *
 * Read the phandle /sys/firmware/devicetree/base/<device>/i2c-eeprom
 * Look for the device at /sys/bus/i2c/devices/<bus>-<addr>/eeprom
 *
 */

/* Extract the reg property (EEPROM I2C slave address) */
static bool get_eeprom_address(const char *eeprom_node, uint8_t *addr)
{
    uint32_t reg;
    if (!dt_node_read_u32(eeprom_node, "reg", &reg))
        return false;
    *addr = (uint8_t)(reg & 0xFF);
    return true;
}

/* Global cached discovered path and offset */
static bool g_inited = false;
static char g_eeprom_sysfs_path[128];
static off_t g_offset = 0;

static bool discover_dm_eeprom(void)
{
    if (g_inited)
        return true;
    DEBUG_PRINTF("Discovering DM I2C EEPROM bootcount device...\n");

    /* Verify required sysfs roots exist */
    struct stat sys_sb;
    if (!dt_root_available() || stat("/sys/bus/i2c/devices", &sys_sb) != 0) {
        DEBUG_PRINTF(" Required sysfs paths missing; DM EEPROM unsupported.\n");
        return false;
    }
    char chosen_bc_path[PATH_MAX];
    if (snprintf(chosen_bc_path, sizeof(chosen_bc_path), DT_ROOT "/chosen/u-boot,bootcount-device") >= (int)sizeof(chosen_bc_path))
        return false;
    uint32_t bc_phandle;
    if (!dt_read_u32(chosen_bc_path, &bc_phandle)) {
        return false; /* No chosen bootcount device */
    }
    DEBUG_PRINTF(" Found bootcount-device phandle %u\n", bc_phandle);
    char bc_node[PATH_MAX];
    if (!dt_find_phandle_node(bc_phandle, bc_node, sizeof(bc_node)))
        return false;
    DEBUG_PRINTF(" Found bootcount node %s\n", bc_node);

    /* Read offset (optional) */
    uint32_t offset = 0;
    dt_node_read_u32(bc_node, "offset", &offset); /* ignore failure => 0 */
    g_offset = (off_t)offset;
    DEBUG_PRINTF(" Using offset 0x%lx\n", (unsigned long)g_offset);

    /* Read i2c-eeprom phandle */
    uint32_t eeprom_phandle;
    if (!dt_node_read_u32(bc_node, "i2c-eeprom", &eeprom_phandle))
        return false;
    DEBUG_PRINTF(" Found i2c-eeprom phandle %u\n", eeprom_phandle);

    char eeprom_node[PATH_MAX];
    if (!dt_find_phandle_node(eeprom_phandle, eeprom_node, sizeof(eeprom_node)))
        return false;
    DEBUG_PRINTF(" Found eeprom node %s\n", eeprom_node);
    uint8_t slave_addr;
    if (!get_eeprom_address(eeprom_node, &slave_addr))
        return false;
    DEBUG_PRINTF(" EEPROM I2C address 0x%02x\n", slave_addr);

    /* Iterate /sys/bus/i2c/devices/<bus-<addr>/ and compare their
     * of_node symlink target to the exact eeprom DT node path we resolved.
     */
    bool matched = false;
    DIR *dev_dir = opendir("/sys/bus/i2c/devices");
    if (!dev_dir)
        return false;
    struct dirent *de2;
    while ((de2 = readdir(dev_dir))) {
        int bus; unsigned int addr4;
        if (sscanf(de2->d_name, "%d-%04x", &bus, &addr4) != 2)
            continue; /* skip non device entries */
        /* Fast reject: address mismatch */
        if ((addr4 & 0xFF) != slave_addr)
            continue;
        DEBUG_PRINTF(" Checking device %s ...\n", de2->d_name);
        char link_path[PATH_MAX];
        if (snprintf(link_path, sizeof(link_path), "/sys/bus/i2c/devices/%s/of_node", de2->d_name) >= (int)sizeof(link_path))
            continue; /* path truncated */
        char *resolved = realpath(link_path, NULL); /* let libc allocate */
        if (!resolved)
            continue;
        bool same = (strcmp(resolved, eeprom_node) == 0);
        free(resolved);
        if (same) {
            snprintf(g_eeprom_sysfs_path, sizeof(g_eeprom_sysfs_path),
                     "/sys/bus/i2c/devices/%s/eeprom", de2->d_name);
            matched = true;
            DEBUG_PRINTF(" Chose EEPROM device %s\n", g_eeprom_sysfs_path);
            break;
        }
    }
    closedir(dev_dir);
    if (!matched)
        return false;

    /* Validate file exists */
    struct stat sb;
    if (stat(g_eeprom_sysfs_path, &sb) != 0)
        return false;

    g_inited = true;
    return true;
}

static int dm_eeprom_open(void)
{
    if (!discover_dm_eeprom())
        return E_DEVICE;
    int fd = open(g_eeprom_sysfs_path, O_RDWR);
    if (fd < 0)
        return E_DEVICE;
    if (lseek(fd, g_offset, SEEK_SET) == -1) {
        close(fd);
        return E_DEVICE;
    }
    return fd;
}

bool dm_eeprom_exists(void)
{
    return discover_dm_eeprom();
}

int dm_eeprom_read_bootcount(uint16_t *val)
{
    int fd = dm_eeprom_open();
    if (fd < 0)
        return fd;

    unsigned char bytes[2];
    if (read(fd, bytes, sizeof(bytes)) != (ssize_t)sizeof(bytes)) {
        close(fd);
        return E_DEVICE;
    }
    close(fd);

    if (bytes[1] != DM_I2C_MAGIC) {
        /* Upstream DM driver resets counter to 0 on invalid magic.
           We have a reset command, so do not write on a read operation
         */
        return E_BADMAGIC;
    }
    *val = bytes[0];
    return 0;
}

int dm_eeprom_write_bootcount(uint16_t val)
{
    int fd = dm_eeprom_open();
    if (fd < 0)
        return fd;
    unsigned char bytes[2];
    bytes[0] = (unsigned char)(val & 0xff);
    bytes[1] = DM_I2C_MAGIC;
    ssize_t written = write(fd, bytes, sizeof(bytes));
    if (written != (ssize_t)sizeof(bytes)) {
        close(fd);
        return E_DEVICE;
    }
    close(fd);
    return 0;
}


