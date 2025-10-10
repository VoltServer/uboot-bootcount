#pragma once

#define DM_EEPROM_NAME "DM I2C EEPROM"

bool dm_eeprom_exists(void);
int dm_eeprom_read_bootcount(uint16_t *val);
int dm_eeprom_write_bootcount(uint16_t val);

int dm_eeprom_read_path(const char *path, off_t offset, uint8_t magic, uint16_t *val);
int dm_eeprom_write_path(const char *path, off_t offset, uint8_t magic, uint16_t val);
