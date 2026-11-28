/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Summary of a panic dump in the device's dedicated flash partition.
 *
 * A summary does not require the original ELF. A symbolized backtrace
 * still needs the exact matching ELF and the tools described in `docs/coredump.md`.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Fields reported by ESP-IDF for one raw dump, meaningful only when present. */
typedef struct
{
    bool present;        /**< A checksum-valid dump was summarized. */
    char task[16];       /* name of the task that crashed */
    uint32_t pc;         /* program counter at the crash */
    uint32_t mcause;     /**< RISC-V mcause or Xtensa EXCCAUSE under the stable archive key. */
    uint32_t mtval;      /**< RISC-V mtval or Xtensa EXCVADDR. */
    uint32_t ra;         /* return address */
    uint32_t sp;         /* stack pointer */
    size_t size;         /**< Size in bytes of the raw dump including its checksum. */
    char elf_sha256[65]; /**< Crashing application's ELF SHA-256 prefix from the dump. */
} coredump_report_t;

/** @brief Clear the destination and read a summary when a valid dump is available.
 * @return True with present false if no checksum-valid dump is available, or
 *         with present true on success. False on invalid output or summary failure.
 */
bool coredump_report_read(coredump_report_t *out);

/** @return True if the actual flashed partition table contains a core dump partition.
 *
 * The application image alone does not determine this because OTA does
 * not update the partition table.
 */
bool coredump_report_supported(void);

/** @brief Log one stored dump summary, normally just after the boot reset-reason line. */
void coredump_report_log_at_boot(void);

/** @return A static exception name for this target architecture, or NULL for an unknown value. */
const char *coredump_report_mcause_name(uint32_t mcause);
