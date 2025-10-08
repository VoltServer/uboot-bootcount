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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/* Root of flattened DT in sysfs (preferred for runtime property access) */
#define DT_ROOT "/sys/firmware/devicetree/base"

/* /proc helper: check if SoC compatible string is present */
bool is_compatible_soc(const char* compat);

/* Returns true if DT_ROOT exists */
bool dt_root_available(void);

/* Read big-endian u32 from a property file path. Returns true on success. */
bool dt_read_u32(const char *path, uint32_t *val);

/* Find node directory (full path) for a given phandle. Returns true on success. */
bool dt_find_phandle_node(uint32_t phandle, char *out, size_t outlen);

/* Read a u32 property (big-endian) from inside a node directory. Returns true on success. */
bool dt_node_read_u32(const char *node_dir, const char *prop, uint32_t *val);

