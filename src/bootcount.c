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
#include "i2c_eeprom.h"

#define DEBUG_PRINTF(...) if (debug) { fprintf( stderr, "DEBUG: " __VA_ARGS__ ); }

#define DT_COMPATIBLE_NODE "/proc/device-tree/soc/compatible"

bool debug = DEBUG;


bool is_ti_am33() {
    FILE *fd = fopen(DT_COMPATIBLE_NODE, "r");

    if (fd == NULL) {
        fprintf(stderr, "No DT node " DT_COMPATIBLE_NODE "\n");
        return false;
    }

    char compatible[100];
    bool is_ti = false;

    if ( fgets(compatible, sizeof(compatible), fd) != NULL ) {
        DEBUG_PRINTF("Read from " DT_COMPATIBLE_NODE ": %s\n", compatible);
        if(strcmp(compatible, "ti,omap-infra") == 0) {
            is_ti = true;
        }
    }

    fclose(fd);
    return is_ti;
}

int platform_detect() {
  if ( is_ti_am33() ) {
    printf("Detected TI AM335x\n");
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

    bool use_eeprom = ! is_ti_am33();

    if ( use_eeprom && ! eeprom_exists() ) {
        fprintf(stderr, "Not TI AM335x and no I2C EEPROM at ");
        fprintf(stderr, EEPROM_PATH "\n", DEFAULT_I2C_BUS, DEFFAULT_I2C_ADDR);
        return E_PLATFORM_UNKNOWN;
    }

    DEBUG_PRINTF("Using EEPROM? %d\n", use_eeprom);

    // no args: read value and print to stdout
    if (argc == 1) {
        DEBUG_PRINTF("Action=read\n");

        uint16_t val = 0;

        if ( use_eeprom ) {
            err = eeprom_read_bootcount(&val);
        }
        else {
            err = am33_read_bootcount(&val);
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

        // "-d" print platform detection to stdout and exit
        if (argc == 2 && strcmp(argv[1], "-d") == 0) {
            DEBUG_PRINTF("Action=detect\n");
            return platform_detect();
        }

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
            if ( use_eeprom ) {
                err = eeprom_write_bootcount(val_arg);
            }
            else {
                err = am33_write_bootcount(val_arg);
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
                    "Read or set the u-boot 'bootcount'.  Presently supports the RTC SCRATCH2\n"
                    "register on TI AM33xx devices, and generic DM I2C EEPROM via /sys/bus/i2c/devices/\n"
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
