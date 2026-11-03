/*
 * Goldfish RTC emulation
 *
 * Copyright (c) 2026 PT Baris Inovasi Ceria
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#ifndef GOLDFISH_RTC_H
#define GOLDFISH_RTC_H

#include "iomem.h"

#define GOLDFISH_RTC_SIZE 0x1000

typedef struct GoldfishRTC GoldfishRTC;

GoldfishRTC *goldfish_rtc_init(PhysMemoryMap *mem_map, uint64_t addr,
                               IRQSignal *irq);
/* fire the alarm if due; returns ms until the alarm (or max_delay) */
int goldfish_rtc_poll(GoldfishRTC *s, int max_delay);

#endif /* GOLDFISH_RTC_H */
