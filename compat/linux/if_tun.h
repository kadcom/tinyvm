/* Copyright (c) 2026 PT Baris Inovasi Ceria
   SPDX-License-Identifier: AGPL-3.0-only */
/* macOS shim for <linux/if_tun.h>: lets temu.c compile; TAP networking is
   Linux-only and fails at runtime on macOS (use eth0 driver "user"). */
#pragma once
#ifdef __APPLE__
#define IFF_TAP   0x0002
#define IFF_NO_PI 0x1000
#define TUNSETIFF 0
#else
#include_next <linux/if_tun.h>
#endif
