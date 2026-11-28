/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Streaming SHA-256 independent of the chip's shared SHA engine.
 *
 * Keep long-running hashes separate from concurrent hardware hashing.
 * Each stream uses only its caller-owned state.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief Running digest state and partial block for one stream. */
typedef struct
{
    uint32_t state[8];
    uint64_t length; /**< Total input bytes before final padding. */
    uint8_t block[64];
    size_t block_len;
} sha256_soft_t;

/** @brief Initialize a caller-owned context for a new stream. */
void sha256_soft_init(sha256_soft_t *ctx);
/** @brief Consume the next len bytes without retaining the caller's buffer. */
void sha256_soft_update(sha256_soft_t *ctx, const uint8_t *data, size_t len);
/** @brief Finalize the context and write the 32-byte digest.
 * @post Initialize the context again before hashing another stream.
 */
void sha256_soft_final(sha256_soft_t *ctx, uint8_t digest[32]);
