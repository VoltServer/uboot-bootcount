#pragma once

#define DEFAULT_I2C_BUS 2
#define DEFFAULT_I2C_ADDR 50
#define DEFAULT_OFFSET 0x100

#define EEPROM_PATH "/sys/bus/i2c/devices/%d-%04d/eeprom"

int eeprom_read_bootcount(uint16_t *val);

int eeprom_write_bootcount(uint16_t val);

bool eeprom_exists(void);
