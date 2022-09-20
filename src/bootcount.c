/**
 * bootcount.c
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
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
//#include <inttypes.h> // used for PRIx32 macro
#include <config.h>

#include "constants.h"
#include "am33xx.h"
#include "stm32mp1.h"
#include "i2c_eeprom.h"

#define DEBUG_PRINTF(...) if (debug) { fprintf( stderr, "DEBUG: " __VA_ARGS__ ); }

#define DT_COMPATIBLE_NODE "/proc/device-tree/compatible"

bool debug = DEBUG;

/**
 * Read /proc/device-tree/compatible to detect hardware platform, which
 * can be used to determine which bootcount strategy to use
 */
bool is_compatible_soc(const char* compat_str) {
    FILE *fd = fopen(DT_COMPATIBLE_NODE, "r");

    if (fd == NULL) {
        fprintf(stderr, "No DT node " DT_COMPATIBLE_NODE "\n");
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

bool is_ti_am33() {
    return is_compatible_soc("ti,am33xx");
}

bool is_stm32mp1() {
    return is_compatible_soc("st,stm32mp153") || is_compatible_soc("st,stm32mp157");
}

int platform_detect() {
    if ( is_ti_am33() ) {
        printf("Detected TI AM335x\n");
        return 0;
    }

    if ( is_stm32mp1() ) {
        printf("Detected STM32MP1\n");
        return 0;
    }

    if ( eeprom_exists() ) {
        printf("Detected I2C EEPROM\n");
        return 0;
    }

    printf("Warning: unknown platform: Not TI AM335x and no I2C EEPROM at ");
    printf(EEPROM_PATH "\n", DEFAULT_I2C_BUS, DEFFAULT_I2C_ADDR);
    return E_PLATFORM_UNKNOWN;
}

int main(int argc, char *argv[]) {
    int err;

    char *debug_env = getenv("DEBUG");

    if (
        debug_env != NULL &&
        (strcmp(debug_env, "1") == 0 || strcmp(debug_env, "true") == 0)
     ) {
        debug = true;
    }
    DEBUG_PRINTF("DEBUG=%s\n", debug_env);

    // "-d" print platform detection to stdout and exit
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        DEBUG_PRINTF("Action=detect\n");
        return platform_detect();
    }

    bool use_ti = is_ti_am33();
    bool use_stm = ! use_ti && is_stm32mp1();

    if ( ! use_ti && ! use_stm && ! eeprom_exists() ) {
        fprintf(stderr, "Not TI AM335x or STM32MP1 and no I2C EEPROM at ");
        fprintf(stderr, EEPROM_PATH "\n", DEFAULT_I2C_BUS, DEFFAULT_I2C_ADDR);
        return E_PLATFORM_UNKNOWN;
    }

    // no args: read value and print to stdout
    if (argc == 1) {
        DEBUG_PRINTF("Action=read\n");

        uint16_t val = 0;

        if ( use_ti ) {
            err = am33_read_bootcount(&val);
        }
        else if ( use_stm ) {
            err = stm32mp1_read_bootcount(&val);
        }
        else {
            err = eeprom_read_bootcount(&val);
        }

        if (err != 0) {
            printf("Error %d\n", err);
            return err;
        }

        printf("%u\n", val);
        return 0;
    }

    else {
        uint16_t val_arg;
        bool write_valid = true;

        // "-r" = Reset bootcount to zero
        if (argc == 2 && strcmp(argv[1], "-r") == 0) {
            DEBUG_PRINTF("Action=reset\n");
            val_arg = 0;
        }

        // "-f" = set bootcount to max, force 'altbootcmd' to run if bootlimit is set
        else if (argc == 2 && strcmp(argv[1], "-f") == 0) {
            DEBUG_PRINTF("Action=force\n");
            val_arg = UINT16_MAX-1;
        }

        // "-s" = set to a specific value
        else if (argc == 3 && strcmp(argv[1], "-s") == 0) {
            DEBUG_PRINTF("Action=set\n");
            val_arg = strtoul(argv[2], NULL, 10);
        }
        else {
            write_valid = false;
        }

        if ( write_valid ) {
            DEBUG_PRINTF("Write %d\n", val_arg);
            if ( use_ti ) {
                err = am33_write_bootcount(val_arg);
            }
            else if ( use_stm ) {
                err = stm32mp1_write_bootcount(val_arg);
            }
            else {
                err = eeprom_write_bootcount(val_arg);
            }
            if (err != 0) {
                printf("Error %d\n", err);
                return err;
            }
            return 0;
        }
    }

    // if we fall through, print help and exit
    fprintf(stderr, "Usage: %s [-r] [-f] [-s <val>]\n\n"
                    "Read or set the u-boot 'bootcount'.  Presently supports the following:\n"
                    "  * RTC SCRATCH2 register on TI AM33xx devices\n"
                    "  * TAMP_BKP21R register on STM32MP1 devices\n"
                    "  * generic DM I2C EEPROM via /sys/bus/i2c/devices/\n"
                    "If invoked without any arguments, this prints the current 'bootcount'\n"
                    "value to stdout.\n\n"
                    "OPTIONS:\n\n"
                    "\t-r\t\tReset the bootcount to 0.  Same as '-s 0'\n\n"
                    "\t-s <val>\tSet the bootcount to the given value.\n\n"
                    "\t-f\t\tForce 'altbootcmd' by setting bootcount to UINT16_MAX - 1\n\n"
                    "\t-d\t\tPrint platform detection details to stdout\n\n"
                    "Package details:\t\t" PACKAGE_STRING "\n"
                    "Bug Reports:\t\t" PACKAGE_BUGREPORT "\n"
                    "Homepage:\t\t" PACKAGE_URL "\n\n", argv[0]);
    return 1;
}
