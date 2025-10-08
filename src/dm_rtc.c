/* Support u-boot rtc bootcount devices via DM RTC.
 * ref: u-boot/drivers/bootcount/rtc.c
 *
 * Example device tree fragment:
 *
 *     chosen {
 *        // see: u-boot/drivers/bootcount/bootcount-uclass.c
 *        u-boot,bootcount-device = &bootcount_rv3028;
 *    };
 *
 *    // Phycore contains an RV-3028-C7 RTC
 *    // ensure CONFIG_RV3028 is enabled in U-Boot config
 *    bootcount_rv3028: bc_rv3028 {
 *        // see: u-boot/drivers/bootcount/rtc.c
 *        compatible = "u-boot,bootcount-rtc";
 *        rtc = <&i2c_som_rtc>;
 *        offset = <0x1F>; // registers 0x1F-0x20 are "User RAM"
 *        // In linux, the rtc-rv3028 driver creates a two-byte nvmem. So in linux the offset is not the same
 *        // as the I2C register offset.  So we use another property to specify the linux,nvmem-offset:
 *        linux,nvmem-offset = <0x00>;
 *        // The rtc-rv3028 driver creates two nvmem devices, one for "User RAM" with type "Battery backed"
 *        // and one for "EEPROM" with type "EEPROM".  We want the "Battery backed" one because the rv3028
 *        // driver for u-boot does not support the EEPROM.  Use this nvmem-type property to select the correct
 *        // nvmem device:
 *        linux,nvmem-type = "Battery backed"
 *    };
 *
 * The underlying RTC device should expose an nvmem provider in Linux
 * which results in a sysfs file:
 *   /sys/bus/nvmem/devices/<device name>/nvmem
 * We store the bootcount (magic + value) at the specified offset.
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

#include "constants.h"
#include "dm_eeprom.h"
#include "dt.h"

#define NVMEM_SYSFS_DEVICES "/sys/bus/nvmem/devices"
#define RTC_MAGIC 0xbc

/* Cached discovery state */
static bool g_inited = false;
static char g_rtc_nvmem_path[128];
static off_t g_offset = 0;

static bool discover_dm_rtc(void)
{
    if (g_inited)
        return true;
    DEBUG_PRINTF("Discovering DM RTC bootcount device...\n");

    char bc_node[PATH_MAX];
    if(!dt_get_chosen_bootcount_node("u-boot,bootcount-rtc", bc_node, sizeof(bc_node))) {
        return false;
    }
    DEBUG_PRINTF(" Found bootcount node %s\n", bc_node);

    // if there is a `linux,nvmem-type` property, use it later to select the correct nvmem device:
    char nvmem_type[64];
    int nvmem_type_len = dt_node_read_str(bc_node, "linux,nvmem-type", nvmem_type, sizeof(nvmem_type));
    if (nvmem_type_len > 0) {
        DEBUG_PRINTF(" found linux,nvmem-type '%s'\n", nvmem_type);
    }

    uint32_t offset = 0;
    dt_node_read_u32(bc_node, "offset", &offset); /* ignore failure => 0 */
    g_offset = (off_t)offset;

    // if there is a `linux,nvmem-offset` property, use it instead of `offset`:
    uint32_t linux_nvram_offset = 0;
    if (dt_node_read_u32(bc_node, "linux,nvmem-offset", &linux_nvram_offset)) {
        g_offset = (off_t)linux_nvram_offset;
        DEBUG_PRINTF(" found linux,nvmem-offset 0x%lx\n", (unsigned long)g_offset);
    }
    DEBUG_PRINTF(" using offset 0x%lx\n", (unsigned long)g_offset);

    /* rtc phandle */
    uint32_t rtc_phandle;
    if (!dt_node_read_u32(bc_node, "rtc", &rtc_phandle))
        return false;
    DEBUG_PRINTF(" rtc phandle %u\n", rtc_phandle);

    char rtc_device_path[PATH_MAX];
    if (!dt_find_phandle_node(rtc_phandle, rtc_device_path, sizeof(rtc_device_path)))
        return false;
    DEBUG_PRINTF(" rtc node %s\n", rtc_device_path);

    /* Iterate nvmem devices to find matching of_node */
    DIR *dev_dir = opendir(NVMEM_SYSFS_DEVICES);
    if (!dev_dir)
        return false;
    struct dirent *de;
    bool matched = false;
    while ((de = readdir(dev_dir))) {
        if (de->d_name[0] == '.')
            continue; /* skip dot entries */

        // construct the base path to the nvmem device: /sys/bus/nvmem/devices/<device name>
        char nvmem_path[PATH_MAX];
        if (snprintf(nvmem_path, sizeof(nvmem_path), NVMEM_SYSFS_DEVICES "/%s", de->d_name) >= (int)sizeof(nvmem_path)) {
            DEBUG_PRINTF(" ERROR Path truncated constructing nvmem path\n");
            continue;
        }

        // find the device under /sys/bus/nvmem/devices whose of_node symlink
        // matches the rtc_device_path we resolved above:
        char link_path[PATH_MAX];
        if (snprintf(link_path, sizeof(link_path), "%s/of_node", nvmem_path) >= (int)sizeof(link_path)) {
            DEBUG_PRINTF(" ERROR Path truncated constructing of_node path\n");
            continue;
        }
        if (!same_fs_node(link_path, rtc_device_path)) {
            continue;
        }
        DEBUG_PRINTF(" Matched device %s\n", link_path);

        // if the dt definition included a `linux,nvmem-type` property, check it matches the nvmem "type"
        // example: /sys/bus/nvmem/devices/rv3028_nvram0/type -> "Battery backed"
        if (nvmem_type_len > 0) {
            char dev_nvmem_type[64];
            int dev_nvmem_type_len = dt_node_read_str(nvmem_path, "type", dev_nvmem_type, sizeof(dev_nvmem_type));
            // trim the trailing newline if present:
            if (dev_nvmem_type_len > 0 && dev_nvmem_type[dev_nvmem_type_len - 1] == '\n') {
                dev_nvmem_type[--dev_nvmem_type_len] = '\0';
            }
            if (dev_nvmem_type_len <= 0 || strcmp(dev_nvmem_type, nvmem_type) != 0) {
                DEBUG_PRINTF(" %s/type '%s' does not match expected '%s', continuing...\n", nvmem_path, dev_nvmem_type, nvmem_type);
                continue;
            }
            DEBUG_PRINTF(" matched nvmem-type '%s'\n", dev_nvmem_type);
        }

        // ensure the device has an nvmem file:
        if (snprintf(g_rtc_nvmem_path, sizeof(g_rtc_nvmem_path), "%s/nvmem", nvmem_path) >= (int)sizeof(g_rtc_nvmem_path)) {
            DEBUG_PRINTF(" ERROR path too long: %s/nvmem\n" , nvmem_path);
            continue;
        }
        struct stat sb;
        if (stat(g_rtc_nvmem_path, &sb) != 0) {
            DEBUG_PRINTF(" WARN nvmem path %s does not exist, continuing...\n", g_rtc_nvmem_path);
            continue;
        }
        /* Ensure the nvmem file has sufficient size for the requested offset:
         * we need 2 bytes at offset (magic + bootcount).
         */
        if (sb.st_size < (off_t)(g_offset + 2)) {
            DEBUG_PRINTF(" ERROR nvmem size %ld too small for offset 0x%lx\n", (long)sb.st_size, (unsigned long)g_offset);
            continue;
        }

        matched = true;
        DEBUG_PRINTF(" Chose RTC nvmem %s\n", g_rtc_nvmem_path);
        break;
    }
    closedir(dev_dir);
    if (!matched)
        return false;

    g_inited = true;
    return true;
}

bool dm_rtc_exists(void)
{
    return discover_dm_rtc();
}

int dm_rtc_read_bootcount(uint16_t *val)
{
    if (!dm_rtc_exists())
        return E_DEVICE;
    return dm_eeprom_read_path(g_rtc_nvmem_path, g_offset, RTC_MAGIC, val);
}

int dm_rtc_write_bootcount(uint16_t val)
{
    if (!dm_rtc_exists())
        return E_DEVICE;
    return dm_eeprom_write_path(g_rtc_nvmem_path, g_offset, RTC_MAGIC, val);
}

