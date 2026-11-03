/*
 * ns16550a UART emulation
 *
 * Copyright (c) 2026 PT Baris Inovasi Ceria
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#ifndef UART16550_H
#define UART16550_H

#include "iomem.h"
#include "virtio.h" /* CharacterDevice */

#define UART16550_SIZE      0x100
#define UART16550_CLOCK_HZ  3686400

typedef struct UART16550 UART16550;

UART16550 *uart16550_init(PhysMemoryMap *mem_map, uint64_t addr,
                          IRQSignal *irq, CharacterDevice *cs);
/* number of bytes the RX FIFO can still take from the host */
int uart16550_get_write_len(UART16550 *s);
/* host -> guest bytes (keyboard input) */
void uart16550_write_data(UART16550 *s, const uint8_t *buf, int len);

#endif /* UART16550_H */
