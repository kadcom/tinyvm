/* Copyright (c) 2026 PT Baris Inovasi Ceria
   SPDX-License-Identifier: AGPL-3.0-only */
/* macOS shim for <sys/statfs.h>: statfs() lives in <sys/mount.h> */
#pragma once
#ifdef __APPLE__
#include <sys/param.h>
#include <sys/mount.h>
#else
#include_next <sys/statfs.h>
#endif
