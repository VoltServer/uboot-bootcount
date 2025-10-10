/* Support u-boot rtc bootcount devices via DM RTC.
 * ref: u-boot/drivers/bootcount/rtc.c
 *
 * Example device tree fragment:
 *
 *     chosen {
 *        // see: u-boot/drivers/bootcount/bootcount-uclass.c
 *        u-boot,bootcount-device = <&bootcount_rv3028>;
 *    };
 *
 *    // Phycore contains an RV-3028-C7 RTC with user EEPROM:
 *    // ensure CONFIG_RV3028 is enabled in U-Boot config
 *    bootcount_rv3028: bc_rv3028 {
 *        // see: u-boot/drivers/bootcount/rtc.c
 *        compatible = "u-boot,bootcount-rtc";
 *        rtc = <&i2c_som_rtc>;
 *        offset = <0x10>; // region 0x00-0x2A is "User EEPROM"
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

    if (!dt_root_available())
        return false;

    char chosen_path[PATH_MAX];
    if (snprintf(chosen_path, sizeof(chosen_path), DT_ROOT "/chosen/u-boot,bootcount-device") >= (int)sizeof(chosen_path))
        return false;

    uint32_t bc_phandle;
    if (!dt_read_u32(chosen_path, &bc_phandle))
        return false;
    DEBUG_PRINTF(" found bootcount-device phandle %u\n", bc_phandle);

    char bc_node[PATH_MAX];
    if (!dt_find_phandle_node(bc_phandle, bc_node, sizeof(bc_node)))
        return false;
    DEBUG_PRINTF(" Found bootcount node %s\n", bc_node);

    /* if the bc_node/compatible does not match "u-boot,bootcount-i2c"
       then this is not the correct driver: */
    char compatible[100];
    int compat_len = dt_node_read_str(bc_node, "compatible", compatible, sizeof(compatible));
    if (compat_len <= 0 || strstr(compatible, "u-boot,bootcount-rtc") == NULL) {
        DEBUG_PRINTF(" Chosen bootcount node is not compatible: '%s'\n", compatible);
        return false;
    }

    uint32_t offset = 0;
    dt_node_read_u32(bc_node, "offset", &offset); /* ignore failure => 0 */
    g_offset = (off_t)offset;
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
        // find the device under /sys/bus/nvmem/devices whose of_node symlink
        // matches the rtc_device_path we resolved above:
        char link_path[PATH_MAX];
        if (snprintf(link_path, sizeof(link_path), NVMEM_SYSFS_DEVICES "/%s/of_node", de->d_name) >= (int)sizeof(link_path)) {
            DEBUG_PRINTF(" ERROR Path truncated constructing of_node path\n");
            continue;
        }
        if (!same_fs_node(link_path, rtc_device_path)) {
            continue;
        }
        DEBUG_PRINTF(" Matched device %s\n", link_path);

        // ensure the device has an nvmem file:
        if (snprintf(g_rtc_nvmem_path, sizeof(g_rtc_nvmem_path), NVMEM_SYSFS_DEVICES "/%s/nvmem", de->d_name) >= (int)sizeof(g_rtc_nvmem_path)) {
            DEBUG_PRINTF(" ERROR path too long: " NVMEM_SYSFS_DEVICES " /%s/nvmem\n" , de->d_name);
            continue;
        }
        struct stat sb;
        if (stat(g_rtc_nvmem_path, &sb) != 0) {
            DEBUG_PRINTF(" WARN nvmem path %s does not exist, continuing...\n", g_rtc_nvmem_path);
            continue; /* skip if /nvmem does not exist */
        }

        /* Ensure the nvmem file has sufficient size for the requested offset:
         * we need 2 bytes at offset (magic + bootcount).
         * Specifically, there are some rtc drivers such as the rv3028 which expose multiple nvmem devices.  One is a
         * "User RAM" which is only two bytes.
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

