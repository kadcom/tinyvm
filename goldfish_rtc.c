/*
 * Goldfish RTC emulation
 *
 * Copyright (c) 2026 PT Baris Inovasi Ceria
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include "cutils.h"
#include "goldfish_rtc.h"

/*
 * Register layout from include/clocksource/timer-goldfish.h, behaviour as
 * used by drivers/rtc/rtc-goldfish.c (Linux 6.12) and QEMU's
 * hw/rtc/goldfish_rtc.c: time in ns since the Unix epoch, reading
 * TIME_LOW latches TIME_HIGH, writing TIME_HIGH then TIME_LOW sets the
 * clock, writing ALARM_HIGH then ALARM_LOW arms the alarm.
 */
#define TIMER_TIME_LOW          0x00
#define TIMER_TIME_HIGH         0x04
#define TIMER_ALARM_LOW         0x08
#define TIMER_ALARM_HIGH        0x0c
#define TIMER_IRQ_ENABLED       0x10
#define TIMER_CLEAR_ALARM       0x14
#define TIMER_ALARM_STATUS      0x18
#define TIMER_CLEAR_INTERRUPT   0x1c

struct GoldfishRTC {
    IRQSignal *irq;
    int64_t offset_ns;     /* guest time - host CLOCK_REALTIME */
    uint32_t time_high;    /* latched by a TIME_LOW read */
    uint32_t set_high;     /* TIME_HIGH written, applied with TIME_LOW */
    uint64_t alarm_ns;
    uint32_t alarm_high;   /* ALARM_HIGH written, applied with ALARM_LOW */
    BOOL alarm_running;
    BOOL irq_enabled;
    BOOL irq_pending;
};

static int64_t host_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

static uint64_t rtc_now(GoldfishRTC *s)
{
    return host_time_ns() + s->offset_ns;
}

static void rtc_update_irq(GoldfishRTC *s)
{
    set_irq(s->irq, s->irq_pending && s->irq_enabled);
}

static void rtc_check_alarm(GoldfishRTC *s)
{
    if (s->alarm_running && rtc_now(s) >= s->alarm_ns) {
        s->alarm_running = FALSE;
        s->irq_pending = TRUE;
        rtc_update_irq(s);
    }
}

static uint32_t rtc_read(void *opaque, uint32_t offset, int size_log2)
{
    GoldfishRTC *s = opaque;
    uint64_t t;

    switch (offset) {
    case TIMER_TIME_LOW:
        t = rtc_now(s);
        s->time_high = t >> 32;
        return (uint32_t)t;
    case TIMER_TIME_HIGH:
        return s->time_high;
    case TIMER_ALARM_LOW:
        return (uint32_t)s->alarm_ns;
    case TIMER_ALARM_HIGH:
        return s->alarm_ns >> 32;
    case TIMER_IRQ_ENABLED:
        return s->irq_enabled;
    case TIMER_ALARM_STATUS:
        return s->alarm_running;
    default:
        return 0;
    }
}

static void rtc_write(void *opaque, uint32_t offset, uint32_t val,
                      int size_log2)
{
    GoldfishRTC *s = opaque;

    switch (offset) {
    case TIMER_TIME_HIGH:
        s->set_high = val;
        break;
    case TIMER_TIME_LOW:
        /* goldfish_rtc_set_time(): HIGH first, then LOW */
        s->offset_ns = (int64_t)(((uint64_t)s->set_high << 32) | val) -
            host_time_ns();
        break;
    case TIMER_ALARM_HIGH:
        s->alarm_high = val;
        break;
    case TIMER_ALARM_LOW:
        s->alarm_ns = ((uint64_t)s->alarm_high << 32) | val;
        s->alarm_running = TRUE;
        rtc_check_alarm(s);
        break;
    case TIMER_IRQ_ENABLED:
        s->irq_enabled = val & 1;
        rtc_update_irq(s);
        break;
    case TIMER_CLEAR_ALARM:
        s->alarm_running = FALSE;
        break;
    case TIMER_CLEAR_INTERRUPT:
        s->irq_pending = FALSE;
        rtc_update_irq(s);
        break;
    default:
        break;
    }
}

int goldfish_rtc_poll(GoldfishRTC *s, int max_delay)
{
    int64_t d;
    rtc_check_alarm(s);
    if (!s->alarm_running)
        return max_delay;
    d = ((int64_t)(s->alarm_ns - rtc_now(s))) / 1000000;
    if (d < 0)
        d = 0;
    return d < max_delay ? (int)d : max_delay;
}

GoldfishRTC *goldfish_rtc_init(PhysMemoryMap *mem_map, uint64_t addr,
                               IRQSignal *irq)
{
    GoldfishRTC *s = mallocz(sizeof(*s));
    s->irq = irq;
    cpu_register_device(mem_map, addr, GOLDFISH_RTC_SIZE, s,
                        rtc_read, rtc_write, DEVIO_SIZE32);
    return s;
}
