#pragma once

#define DM_RTC_NAME "DM RTC NVMEM"

bool dm_rtc_exists(void);
int dm_rtc_read_bootcount(uint16_t *val);
int dm_rtc_write_bootcount(uint16_t val);
