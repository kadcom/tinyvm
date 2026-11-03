/* Copyright (c) 2026 PT Baris Inovasi Ceria
   SPDX-License-Identifier: AGPL-3.0-only */
/* macOS shim for <sys/sysmacros.h>: major()/minor() are in <sys/types.h> */
#pragma once
#ifdef __APPLE__
#include <sys/types.h>
#else
#include_next <sys/sysmacros.h>
#endif
