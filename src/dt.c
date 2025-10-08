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
