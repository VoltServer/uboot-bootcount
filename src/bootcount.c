/**
 * bootcount.c
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
#include <sys/mman.h>
#include <unistd.h>
//#include <inttypes.h> // used for PRIx32 macro
#include <config.h>

#include "constants.h"
#include "dt.h"
#include "am33xx.h"
#include "stm32mp1.h"
#include "i2c_eeprom.h"

struct platform {
    const char *name;
    bool (*detect)();
    int (*read_bootcount)(uint16_t *val);
    int (*write_bootcount)(uint16_t val);
};

bool debug = DEBUG;

bool is_ti_am33() {
    return is_compatible_soc("ti,am33xx");
}

bool is_stm32mp1() {
    return is_compatible_soc("st,stm32mp153") || is_compatible_soc("st,stm32mp157");
}
static const struct platform platforms[] = {
    {.name = AM33_PLAT_NAME,
     .detect = is_ti_am33,
     .read_bootcount = am33_read_bootcount,
     .write_bootcount = am33_write_bootcount
    },
    {.name = STM32MP1_PLAT_NAME,
     .detect = is_stm32mp1,
     .read_bootcount = stm32mp1_read_bootcount,
     .write_bootcount = stm32mp1_write_bootcount
    },
    {.name = EEPROM_NAME,
     .detect = eeprom_exists,
     .read_bootcount = eeprom_read_bootcount,
     .write_bootcount = eeprom_write_bootcount
    },
    {.name = NULL} /* sentinel */
};

static int platform_detect(const struct platform **platform, bool verbose) {
    const struct platform *plat;
    int i;

    for (i = 0; platforms[i].name; i++) {
        plat = &platforms[i];
        if (plat->detect()) {
            if (platform)
                *platform = plat;

            if (verbose)
                printf("Detected %s\n", plat->name);

            return 0;
        }
    }

    fprintf(stderr, "Warning: unknown platform\n");
    fprintf(stderr, "Current support is for:\n");
    for (i = 0; platforms[i].name; i++) {
        plat = &platforms[i];
        fprintf(stderr, " * %s", plat->name);
        if (!strcmp(plat->name, EEPROM_NAME))
            fprintf(stderr, " at " EEPROM_PATH,
                    DEFAULT_I2C_BUS, DEFFAULT_I2C_ADDR);

        fprintf(stderr, "\n");
    }

    return E_PLATFORM_UNKNOWN;
}

int main(int argc, char *argv[]) {
    int err;
    bool detect_only;
    const struct platform *plat;

    char *debug_env = getenv("DEBUG");

    if (
        debug_env != NULL &&
        (strcmp(debug_env, "1") == 0 || strcmp(debug_env, "true") == 0)
     ) {
        debug = true;
    }
    DEBUG_PRINTF("DEBUG=%s\n", debug_env);

    detect_only = argc == 2 && strcmp(argv[1], "-d") == 0;
    err = platform_detect(&plat, detect_only);

    // "-d" print platform detection to stdout and exit
    if (detect_only) {
        DEBUG_PRINTF("Action=detect\n");
        return err;
    }

    if (err)
        return err;

    // no args: read value and print to stdout
    if (argc == 1) {
        DEBUG_PRINTF("Action=read\n");

        uint16_t val = 0;

        err = plat->read_bootcount(&val);
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

            err = plat->write_bootcount(val_arg);
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
