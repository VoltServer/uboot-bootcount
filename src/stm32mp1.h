# pragma once

#define STM32MP1_PLAT_NAME "STM32MP1"

// See https://wiki.st.com/stm32mpu/wiki/STM32MP15_backup_registers#BOOT_COUNTER
#define TAMP_BKP0R 0x5C00A100ul
#define TAMP_BKP21R_OFFSET 0x54ul
#define REG_SIZE             4ul          // registers are 4 bytes/ 32bit

int stm32mp1_read_bootcount(uint16_t* val);

int stm32mp1_write_bootcount(uint16_t val);
