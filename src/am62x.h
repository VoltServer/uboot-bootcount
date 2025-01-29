# pragma once

#include <stdbool.h>

#define AM62_PLAT_NAME "TI AM62x"

bool is_am62();
int am62_read_bootcount(uint16_t* val);
int am62_write_bootcount(uint16_t val);
