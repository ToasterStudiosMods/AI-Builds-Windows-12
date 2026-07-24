/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * rtc.c — MC146818 CMOS real-time clock (ports 0x70 index, 0x71 data)
 * ==========================================================================*/

#include "rtc.h"
#include "io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int update_in_progress(void)
{
    outb(CMOS_ADDR, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

static uint8_t bcd2bin(uint8_t v)
{
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

void rtc_read(struct rtc_time *t)
{
    while (update_in_progress())
        ;

    uint8_t sec   = cmos_read(0x00);
    uint8_t min   = cmos_read(0x02);
    uint8_t hour  = cmos_read(0x04);
    uint8_t day   = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year  = cmos_read(0x09);
    uint8_t regB  = cmos_read(0x0B);

    if (!(regB & 0x04)) {          /* values are BCD — convert */
        sec   = bcd2bin(sec);
        min   = bcd2bin(min);
        hour  = (uint8_t)(((hour & 0x0F) + (((hour & 0x70) >> 4) * 10)) | (hour & 0x80));
        day   = bcd2bin(day);
        month = bcd2bin(month);
        year  = bcd2bin(year);
    }

    if (!(regB & 0x02) && (hour & 0x80)) {   /* 12-hour mode, PM bit set */
        hour = (uint8_t)(((hour & 0x7F) + 12) % 24);
    }

    t->sec   = sec;
    t->min   = min;
    t->hour  = hour;
    t->day   = day;
    t->month = month;
    t->year  = (uint16_t)(2000 + year);
}
