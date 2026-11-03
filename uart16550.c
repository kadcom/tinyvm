/*
 * ns16550a UART emulation
 *
 * Copyright (c) 2026 PT Baris Inovasi Ceria
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "cutils.h"
#include "uart16550.h"

/*
 * Register model follows the PC16550D datasheet and what Linux
 * (drivers/tty/serial/8250/8250_port.c) and OpenSBI (lib/utils/serial/
 * uart8250.c) actually touch. Transmission is instantaneous: THR/TSR are
 * always empty, so LSR.THRE and LSR.TEMT always read 1.
 */

/* register offsets (reg-shift 0) */
#define UART_RX   0 /* in:  receive buffer (DLAB=0) */
#define UART_TX   0 /* out: transmit holding (DLAB=0) */
#define UART_DLL  0 /* divisor latch low (DLAB=1) */
#define UART_IER  1 /* interrupt enable (DLAB=0) */
#define UART_DLM  1 /* divisor latch high (DLAB=1) */
#define UART_IIR  2 /* in:  interrupt identification */
#define UART_FCR  2 /* out: FIFO control */
#define UART_LCR  3
#define UART_MCR  4
#define UART_LSR  5
#define UART_MSR  6
#define UART_SCR  7

#define IER_RDI   0x01
#define IER_THRI  0x02
#define IER_RLSI  0x04
#define IER_MSI   0x08

#define IIR_NO_INT 0x01
#define IIR_MSI    0x00
#define IIR_THRI   0x02
#define IIR_RDI    0x04
#define IIR_RLSI   0x06
#define IIR_FIFO_ENABLED 0xc0

#define FCR_ENABLE_FIFO 0x01
#define FCR_CLEAR_RCVR  0x02
#define FCR_CLEAR_XMIT  0x04

#define LCR_DLAB  0x80

#define MCR_DTR   0x01
#define MCR_RTS   0x02
#define MCR_OUT1  0x04
#define MCR_OUT2  0x08
#define MCR_LOOP  0x10

#define LSR_DR    0x01
#define LSR_OE    0x02
#define LSR_THRE  0x20
#define LSR_TEMT  0x40

#define MSR_DCTS  0x01
#define MSR_DDSR  0x02
#define MSR_TERI  0x04
#define MSR_DDCD  0x08
#define MSR_CTS   0x10
#define MSR_DSR   0x20
#define MSR_RI    0x40
#define MSR_DCD   0x80

#define RX_FIFO_SIZE 16

struct UART16550 {
    IRQSignal *irq;
    CharacterDevice *cs;
    uint8_t ier, lcr, mcr, lsr_err, msr_delta, scr, fcr;
    uint8_t dll, dlm;
    BOOL thr_ipending; /* THR-empty interrupt pending */
    uint8_t rx_fifo[RX_FIFO_SIZE];
    int rx_count, rx_rindex;
};

static int uart_rx_free(UART16550 *s)
{
    /* without FIFO the receiver holds a single byte */
    int size = (s->fcr & FCR_ENABLE_FIFO) ? RX_FIFO_SIZE : 1;
    return size - s->rx_count;
}

static void uart_rx_push(UART16550 *s, uint8_t c)
{
    if (uart_rx_free(s) <= 0) {
        s->lsr_err |= LSR_OE;
        return;
    }
    s->rx_fifo[(s->rx_rindex + s->rx_count) % RX_FIFO_SIZE] = c;
    s->rx_count++;
}

static uint8_t uart_get_msr(UART16550 *s)
{
    uint8_t msr;
    if (s->mcr & MCR_LOOP) {
        /* loopback: RTS->CTS, DTR->DSR, OUT1->RI, OUT2->DCD (autoconfig
           in 8250_port.c checks MCR=LOOP|0x0a gives MSR&0xf0 == 0x90) */
        msr = 0;
        if (s->mcr & MCR_RTS)  msr |= MSR_CTS;
        if (s->mcr & MCR_DTR)  msr |= MSR_DSR;
        if (s->mcr & MCR_OUT1) msr |= MSR_RI;
        if (s->mcr & MCR_OUT2) msr |= MSR_DCD;
    } else {
        /* a terminal is always attached */
        msr = MSR_DCD | MSR_DSR | MSR_CTS;
    }
    return msr | s->msr_delta;
}

static uint8_t uart_get_iir_id(UART16550 *s)
{
    if ((s->ier & IER_RLSI) && (s->lsr_err & LSR_OE))
        return IIR_RLSI;
    if ((s->ier & IER_RDI) && s->rx_count > 0)
        return IIR_RDI;
    if ((s->ier & IER_THRI) && s->thr_ipending)
        return IIR_THRI;
    if ((s->ier & IER_MSI) && (s->msr_delta & 0x0f))
        return IIR_MSI;
    return IIR_NO_INT;
}

static void uart_update_irq(UART16550 *s)
{
    /* level-triggered line towards the PLIC */
    set_irq(s->irq, uart_get_iir_id(s) != IIR_NO_INT);
}

static void uart_tx(UART16550 *s, uint8_t c)
{
    if (s->mcr & MCR_LOOP) {
        uart_rx_push(s, c);
    } else {
        s->cs->write_data(s->cs->opaque, &c, 1);
    }
    /* transmission is instantaneous: THR is empty again */
    s->thr_ipending = TRUE;
}

static uint32_t uart_read(void *opaque, uint32_t offset, int size_log2)
{
    UART16550 *s = opaque;
    uint32_t val;

    switch (offset) {
    case UART_RX:
        if (s->lcr & LCR_DLAB) {
            val = s->dll;
        } else {
            if (s->rx_count > 0) {
                val = s->rx_fifo[s->rx_rindex];
                s->rx_rindex = (s->rx_rindex + 1) % RX_FIFO_SIZE;
                s->rx_count--;
            } else {
                val = 0;
            }
            uart_update_irq(s);
        }
        break;
    case UART_IER:
        if (s->lcr & LCR_DLAB)
            val = s->dlm;
        else
            val = s->ier;
        break;
    case UART_IIR:
        val = uart_get_iir_id(s);
        /* reading IIR while it reports THRI clears that source */
        if (val == IIR_THRI)
            s->thr_ipending = FALSE;
        if (s->fcr & FCR_ENABLE_FIFO)
            val |= IIR_FIFO_ENABLED;
        uart_update_irq(s);
        break;
    case UART_LCR:
        val = s->lcr;
        break;
    case UART_MCR:
        val = s->mcr;
        break;
    case UART_LSR:
        val = LSR_THRE | LSR_TEMT | s->lsr_err;
        if (s->rx_count > 0)
            val |= LSR_DR;
        s->lsr_err = 0; /* error bits clear on read */
        uart_update_irq(s);
        break;
    case UART_MSR:
        val = uart_get_msr(s);
        s->msr_delta = 0;
        uart_update_irq(s);
        break;
    case UART_SCR:
        val = s->scr;
        break;
    default:
        val = 0;
        break;
    }
    return val;
}

static void uart_write(void *opaque, uint32_t offset, uint32_t val,
                       int size_log2)
{
    UART16550 *s = opaque;
    uint8_t v = val;

    switch (offset) {
    case UART_TX:
        if (s->lcr & LCR_DLAB) {
            s->dll = v;
        } else {
            s->thr_ipending = FALSE;
            uart_tx(s, v);
        }
        break;
    case UART_IER:
        if (s->lcr & LCR_DLAB) {
            s->dlm = v;
        } else {
            uint8_t old = s->ier;
            s->ier = v & 0x0f;
            /* enabling THRI while THR is empty raises it (16550 behaviour,
               relied on by the UART_BUG_THRE test in serial8250_do_startup) */
            if (!(old & IER_THRI) && (s->ier & IER_THRI))
                s->thr_ipending = TRUE;
        }
        break;
    case UART_FCR:
        if (v & FCR_CLEAR_RCVR) {
            s->rx_count = 0;
            s->rx_rindex = 0;
        }
        if ((v ^ s->fcr) & FCR_ENABLE_FIFO) {
            /* changing FIFO mode resets both FIFOs */
            s->rx_count = 0;
            s->rx_rindex = 0;
        }
        s->fcr = v & 0xc9; /* keep enable, DMA mode and trigger level */
        break;
    case UART_LCR:
        s->lcr = v;
        break;
    case UART_MCR:
        s->mcr = v & 0x1f;
        break;
    case UART_SCR:
        s->scr = v;
        break;
    default: /* LSR/MSR are read-only */
        break;
    }
    uart_update_irq(s);
}

int uart16550_get_write_len(UART16550 *s)
{
    if (s->mcr & MCR_LOOP)
        return 0;
    return uart_rx_free(s);
}

void uart16550_write_data(UART16550 *s, const uint8_t *buf, int len)
{
    int i;
    for (i = 0; i < len; i++)
        uart_rx_push(s, buf[i]);
    uart_update_irq(s);
}

UART16550 *uart16550_init(PhysMemoryMap *mem_map, uint64_t addr,
                          IRQSignal *irq, CharacterDevice *cs)
{
    UART16550 *s = mallocz(sizeof(*s));
    s->irq = irq;
    s->cs = cs;
    s->thr_ipending = FALSE;
    /* byte access per DT reg-io-width = 1; 32-bit accepted too */
    cpu_register_device(mem_map, addr, UART16550_SIZE, s,
                        uart_read, uart_write, DEVIO_SIZE8 | DEVIO_SIZE32);
    return s;
}
