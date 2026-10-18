/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#pragma once
#include_next <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
/* Windows has no POSIX permission argument. */
#define mkdir(path, ...) _mkdir(path)
#endif
