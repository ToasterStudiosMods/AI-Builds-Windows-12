/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/rtc.h — CMOS real-time clock
 * ==========================================================================*/

#ifndef AURELIAN_RTC_H
#define AURELIAN_RTC_H

#include <stdint.h>

struct rtc_time {
    uint8_t  sec, min, hour, day, month;
    uint16_t year;
};

void rtc_read(struct rtc_time *t);

#endif /* AURELIAN_RTC_H */
