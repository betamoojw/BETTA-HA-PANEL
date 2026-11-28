/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Device identity and license signature verification interface.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LICENSE_DEVICE_ID_LENGTH 64
#define LICENSE_FILE_LENGTH      200
#define LICENSE_FILE_MAX         (LICENSE_FILE_LENGTH + 2)
/** @brief Derive a stable device ID from the chip's eFuse MAC.
 * @return True with 64 lowercase hex digits and a NUL in out.
 */
bool license_device_id(const uint8_t mac[6], char out[65]);
/** @return True when the configured public key can be loaded for signature verification. */
bool license_public_key_valid(const char *public_hex);
/** @brief Verify a license record for the exact expected device.
 *
 * The record is `BETTA1:<64 lowercase hex device ID>:<128 lowercase hex raw P-256 signature>`.
 *
 * @param public_hex Configured public verification key.
 * @param device_id Expected NUL-terminated device identity.
 * @param key Borrowed license bytes; no terminator is required.
 * @param length Record length in bytes, optionally including one line ending.
 * @retval 1 Valid device-bound signature.
 * @retval 0 Malformed record, wrong device, or invalid signature.
 * @retval -1 Cryptographic operation failed.
 */
int license_key_verify(const char *public_hex, const char device_id[65], const void *key, size_t length);
