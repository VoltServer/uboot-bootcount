# pragma once

#include <stdbool.h>

#define AM33_PLAT_NAME "TI AM335x"

bool is_ti_am33();
int am33_read_bootcount(uint16_t* val);
int am33_write_bootcount(uint16_t val);
