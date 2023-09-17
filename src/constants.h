
#pragma once

#include <stdbool.h>

#define DEBUG false
#define BOOTCOUNT_MAGIC   0xB001C041ul    // from u-boot include/common.h

#define E_BADMAGIC -1
#define E_DEVICE -2
#define E_PLATFORM_UNKNOWN -3
#define E_WRITE_FAILED -4

#define DEBUG_PRINTF(...) if (debug) { fprintf( stderr, "DEBUG: " __VA_ARGS__ ); }

extern bool debug;
