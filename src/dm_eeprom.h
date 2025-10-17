#pragma once

#define DM_EEPROM_NAME "DM I2C EEPROM"

bool dm_eeprom_exists(void);
int dm_eeprom_read_bootcount(uint16_t *val);
int dm_eeprom_write_bootcount(uint16_t val);
