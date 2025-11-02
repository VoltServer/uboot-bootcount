/**
 * Device tree utils
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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>

#include "constants.h"
#include "dt.h"

#define DT_COMPATIBLE_NODE "/proc/device-tree/compatible"

/**
 * Read /proc/device-tree/compatible to detect hardware platform, which
 * can be used to determine which bootcount strategy to use
 */
/* Static cache of the compatible property (loaded lazily). If compat_len == 0 we try again. */
static char compat_buf[512];
static size_t compat_len = 0;

static void read_compatible_node(void)
{
    if (compat_len != 0)
        return; /* already loaded */
    FILE *fd = fopen(DT_COMPATIBLE_NODE, "rb");
    if (!fd)
        return;
    compat_len = fread(compat_buf, 1, sizeof(compat_buf) - 1, fd);
    fclose(fd);
    if (compat_len == 0)
        return; /* leave compat_len == 0 so we may retry later */
    if (compat_len == sizeof(compat_buf) - 1) {
        fprintf(stderr, "Warning: compat string " DT_COMPATIBLE_NODE " truncated to %zu\n", sizeof(compat_buf));
    }
    DEBUG_PRINTF("Read from " DT_COMPATIBLE_NODE ":\n");
    compat_buf[compat_len] = '\0'; /* ensure it is null terminated */
    char *ptr = compat_buf;
    while (ptr < compat_buf + compat_len && *ptr) {
        DEBUG_PRINTF("  %s\n", ptr);
        ptr += strlen(ptr) + 1;
    }
}

bool is_compatible_soc(const char* compat_str) {
    read_compatible_node();
    if (compat_len == 0)
        return false;
    for (char *p = compat_buf; p < compat_buf + compat_len && *p; p += strlen(p) + 1) {
        if (strstr(p, compat_str) != NULL) {
            DEBUG_PRINTF("   Found! %s\n", compat_str);
            return true;
        }
    }
    return false;
}

bool dt_root_available(void)
{
    struct stat sb;
    return (stat(DT_ROOT, &sb) == 0 && S_ISDIR(sb.st_mode));
}

bool dt_read_u32(const char *path, uint32_t *val)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    unsigned char b[4];
    size_t r = fread(b, 1, 4, f);
    fclose(f);
    if (r != 4)
        return false;

    *val = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    return true;
}

/* Recursive directory traversal to locate a node directory that owns a given phandle */
static bool dt_scan_dir_for_phandle(const char *dir, uint32_t target,
                                    char *out, size_t outlen, int depth)
{
    if (depth > 8) /* safety recursion limit */
        return false;

    DIR *d = opendir(dir);
    if (!d)
        return false;

    struct dirent *e;
    bool found = false;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue; /* skip dot entries */

        char path[PATH_MAX];
        int plen = snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (plen < 0 || plen >= (int)sizeof(path))
            continue; /* truncated */

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
            continue; /* not a DT node directory */

        char ph_path[PATH_MAX];
        uint32_t val;

        /* Try primary 'phandle' */
        plen = snprintf(ph_path, sizeof(ph_path), "%s/phandle", path);
        if (plen >= 0 && plen < (int)sizeof(ph_path)) {
            if (dt_read_u32(ph_path, &val) && val == target) {
                strncpy(out, path, outlen - 1);
                out[outlen - 1] = 0;
                found = true;
                break;
            }
        }

        /* Try legacy 'linux,phandle' */
        plen = snprintf(ph_path, sizeof(ph_path), "%s/linux,phandle", path);
        if (plen >= 0 && plen < (int)sizeof(ph_path)) {
            if (dt_read_u32(ph_path, &val) && val == target) {
                strncpy(out, path, outlen - 1);
                out[outlen - 1] = 0;
                found = true;
                break;
            }
        }

        /* Recurse if not matched here */
        if (dt_scan_dir_for_phandle(path, target, out, outlen, depth + 1)) {
            found = true;
            break;
        }
    }

    closedir(d);
    return found;
}

bool dt_find_phandle_node(uint32_t phandle, char *out, size_t outlen)
{
    return dt_scan_dir_for_phandle(DT_ROOT, phandle, out, outlen, 0);
}

bool dt_node_read_u32(const char *node_dir, const char *prop, uint32_t *val)
{
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s", node_dir, prop);
    if (n < 0 || n >= (int)sizeof(path))
        return false;
    return dt_read_u32(path, val);
}

/*
 * dt_node_read_str
 * Returns: number of bytes read (excluding terminator) on success.
 *          -E_DEVICE (re-using existing error codes) on I/O failure or empty.
 * NOTE: The string is always null-terminated on success (and on partial reads).
 */
int dt_node_read_str(const char *node_dir, const char *prop, char *out, size_t outlen)
{
    if (!out || outlen == 0)
        return E_DEVICE;

    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s", node_dir, prop);
    if (n < 0 || n >= (int)sizeof(path))
        return E_DEVICE;

    FILE *f = fopen(path, "rb");
    if (!f)
        return E_DEVICE;

    size_t r = fread(out, 1, outlen - 1, f);
    fclose(f);
    if (r == 0) {
        out[0] = 0;
        return E_DEVICE;
    }
    out[r] = 0;
    return (int)r;
}

/* Compare two filesystem objects for identity (same underlying node). */
bool same_fs_node(const char *a, const char *b)
{
    struct stat sa, sb;
    if (stat(a, &sa) != 0)
        return false;
    if (stat(b, &sb) != 0)
        return false;
    return (sa.st_dev == sb.st_dev) && (sa.st_ino == sb.st_ino);
}

bool dt_get_chosen_bootcount_node(const char *compat_str, char* bc_node, size_t bc_node_len)
{
    if (!dt_root_available())
        return false;

    char bc_path[128];
    if (dt_node_read_str(DT_ROOT "/chosen", "u-boot,bootcount-device", bc_path, sizeof(bc_path))) {
        DEBUG_PRINTF(" Found chosen/u-boot,bootcount-device %s\n", bc_path);
        if (snprintf(bc_node, bc_node_len, DT_ROOT "%s", bc_path) >= (int)bc_node_len) {
            DEBUG_PRINTF(" ERROR Path truncated building device node path for %s\n", bc_path);
            return false;
        }
        // else bc_node is set to the full path of the chosen bootcount device node
    }
    else {
      // find the first device with compatible = 'u-boot,bootcount*' and see if it matches our compat_str
      if (!dt_find_compatible_node("u-boot,bootcount", bc_node, sizeof(bc_node))) {
          DEBUG_PRINTF(" No compatible node found for bootcount driver '%s'\n", compat_str);
          return false;
      }
      // else bc_node is set to the full path of the first matching bootcount device node
    }

    /* if the bc_node/compatible does not match `compat_str`, then this is not the correct driver: */
    char compatible[128];
    int bc_compat_len = dt_node_read_str(bc_node, "compatible", compatible, sizeof(compatible));
    if (bc_compat_len <= 0 || strstr(compatible, compat_str) == NULL) {
        DEBUG_PRINTF(" Found bootcount node is not compatible: '%s'\n", compatible);
        return false;
    }
    return true;
}

/* Recursive directory traversal to locate a device node with a /compatible property that matches compat_str  */
static bool dt_scan_dir_for_compatible(const char *dir, const char *compat_str, char *out, size_t outlen, int depth)
{
    if (depth > 8) /* safety recursion limit */
        return false;

    DIR *d = opendir(dir);
    if (!d)
        return false;

    struct dirent *e;
    bool found = false;
    while (!found && (e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue; /* skip dot entries */

        char path[PATH_MAX];
        int plen = snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (plen < 0 || plen >= (int)sizeof(path))
            continue; /* truncated */

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
            continue; /* not a DT node directory */

        /* Try to read a 'compatible' property */
        char compat_val[255];
        int dt_compat_len = dt_node_read_str(path, "compatible", compat_val, sizeof(compat_val));
        // match if compat_str is a substring of compat_val:
        if (dt_compat_len > 0 && strncmp(compat_val, compat_str, strlen(compat_str)) == 0) {
            strncpy(out, path, outlen - 1);
            out[outlen - 1] = 0;
            found = true;
            break;
        }

        /* Recurse if not matched here */
        if (dt_scan_dir_for_compatible(path, compat_str, out, outlen, depth + 1)) {
            found = true;
            break;
        }
    }

    closedir(d);
    return found;
}

bool dt_find_compatible_node(const char * compat_str, char *out, size_t outlen)
{
    return dt_scan_dir_for_compatible(DT_ROOT, compat_str, out, outlen, 0);
}
