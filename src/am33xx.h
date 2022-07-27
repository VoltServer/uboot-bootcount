# pragma once

// See u-boot arch/arm/include/asm/davinci_rtc.h:
#define RTCSS                0x44E3E000ul
#define SCRATCH2_REG_OFFSET  0x68ul
#define REG_SIZE             4ul          // registers are 4 bytes/ 32bit

#define KICK0R_REG_OFFSET 0x6Cul          // see PDF section 20.3.5.23
#define KICK1R_REG_OFFSET 0x70ul
#define KICK0_MAGIC       0x83E70B13ul
#define KICK1_MAGIC       0x95A4F1E0ul

int am33_read_bootcount(uint16_t* val);

int am33_write_bootcount(uint16_t val);
