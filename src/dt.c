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
bool is_compatible_soc(const char* compat_str) {
    FILE *fd = fopen(DT_COMPATIBLE_NODE, "r");

    if (fd == NULL) {
        fprintf(stderr, "No DT node " DT_COMPATIBLE_NODE
                " while searching for \"%s\"\n", compat_str);
        return false;
    }

    char compatible[100];
    bool is_match = false;
    int bytes_read = 0;

    while (feof(fd) == 0 && ferror(fd) == 0 && ! is_match) {
        // Note: if the 'compatible' node specifies multiple strings, they will be
        // null-delineated. Therefore fread() can be used to read the whole file however
        // string functions like like strstr() will only consider data up to the first null byte.
        // We need to continue comparing strings up to bytes_read

        if ( (bytes_read = fread(compatible, 1, sizeof(compatible)-1, fd)) > 0 ) {
            compatible[bytes_read] = 0; // null-terminate the full string
            char *ptr = compatible;
            while( ptr < compatible+bytes_read ) {
                DEBUG_PRINTF("Read from " DT_COMPATIBLE_NODE ": %s\n", ptr);
                if(strstr(ptr, compat_str) != NULL) {
                    DEBUG_PRINTF("   Found! %s\n", compat_str);
                    is_match = true;
                    break;
                }
                ptr += strlen(ptr) + 1;
            }
        }
    }

    fclose(fd);
    return is_match;
}
