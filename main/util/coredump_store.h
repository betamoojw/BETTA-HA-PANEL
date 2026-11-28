/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Filesystem-only core dump store with stable IDs and bounded retention.
 *
 * Initialize before HTTP starts. Store operations have no internal locking;
 * after device startup the HTTP task serializes archive access.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "util/coredump_diagnostics.h"

/** @brief Maximum number of retained reports. */
#define COREDUMP_STORE_LIMIT 2
/** @brief Maximum supported raw dump length in bytes. */
#define COREDUMP_STORE_MAX_BYTES (256U * 1024U)

/** @brief Report metadata associated with one raw dump. */
typedef struct
{
    char id[65];         /**< Lowercase SHA-256 of the raw dump, independent of import time. */
    char elf_sha256[65]; /**< Hash or prefix of the ELF that crashed, not the current build. */
    char task[16];
    char project[33];
    char version[33];
    char git_commit[33];
    char build_date[17];
    char build_time[17];
    char panic_reason[161];
    coredump_diagnostic_t diagnostic;
    uint32_t pc;
    uint32_t mcause;
    uint32_t size;
    uint32_t boot_seq;
    uint32_t archive_seq; /**< Sequence assigned by the archive on a new import, independent of wall time. */
    int64_t crashed_at;   /**< Approximate crash epoch from the last breadcrumb, or zero if unknown. */
} coredump_record_t;

/** @brief Cached metadata for a single initialized archive directory. */
typedef struct
{
    char directory[96];
    coredump_record_t records[COREDUMP_STORE_LIMIT]; /**< Newest first by archive sequence. */
    size_t count;
} coredump_store_t;

/** @brief Read exactly size raw bytes at the given byte offset, or report failure. */
typedef bool (*coredump_reader_t)(void *context, size_t offset, void *buffer, size_t size);

/** @brief Open or create the directory, recover valid records, and prune older verified entries. */
bool coredump_store_init(coredump_store_t *store, const char *directory);
/** @return True for an exact 64-character lowercase hexadecimal report ID. */
bool coredump_store_id_valid(const char *id);
/** @brief Hash a bounded raw dump in chunks, writing the lowercase ID only on success. */
bool coredump_store_hash(coredump_reader_t read, void *context, size_t size, char id[65]);
/** @brief Commit a new report by flushing, verifying, and renaming a temporary file before pruning.
 *
 * For an existing ID, verify the existing file and preserve its metadata.
 * An error during pruning can leave an already committed report in place.
 */
bool coredump_store_put(coredump_store_t *store, const coredump_record_t *record, coredump_reader_t read, void *context);
/** @brief Read only the requested byte range of the raw payload, not the archive header. */
bool coredump_store_read(const coredump_store_t *store, const char *id, size_t offset, void *buffer, size_t size);
/** @brief Remove the exact retained report and update the cache only after file deletion. */
bool coredump_store_delete(coredump_store_t *store, const char *id);
