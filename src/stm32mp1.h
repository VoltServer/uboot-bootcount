# pragma once

#include <stdbool.h>

#define STM32MP1_PLAT_NAME "STM32MP1"

bool is_stm32mp1();
int stm32mp1_read_bootcount(uint16_t* val);
int stm32mp1_write_bootcount(uint16_t val);
