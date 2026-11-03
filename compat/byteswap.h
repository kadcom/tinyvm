/* Copyright (c) 2026 PT Baris Inovasi Ceria
   SPDX-License-Identifier: AGPL-3.0-only */
/* macOS shim for glibc <byteswap.h> (only on the include path on macOS) */
#pragma once
#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#else
#include_next <byteswap.h>
#endif
