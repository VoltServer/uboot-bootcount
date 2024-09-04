/**
 * Access and reset u-boot's "bootcount" counter for IMX93 platform
 *
 * This file is part of the uboot-bootcount (https://github.com/VoltServer/uboot-bootcount).
 *
 * Copyright (c) 2024 ELCO Elettronica Automation s.r.l., Stefano Costa <s.costa@elcoelettronica.it>
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

# pragma once

#define IMX93_PLAT_NAME "IMX93"

bool is_imx93();
int imx93_read_bootcount(uint16_t* val);
int imx93_write_bootcount(uint16_t val);

