/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LICENSE_DEVICE_ID_LENGTH 64
#define LICENSE_FILE_LENGTH 200
#define LICENSE_FILE_MAX (LICENSE_FILE_LENGTH + 2)
/* BETTA1:<64 lowercase hex device ID>:<128 lowercase hex raw P-256 signature>. */
bool license_device_id(const uint8_t mac[6], char out[65]);
bool license_public_key_valid(const char *public_hex);
/* 1 valid; 0 malformed/wrong device/invalid signature; -1 crypto failure. */
int license_key_verify(const char *public_hex, const char device_id[65], const void *key, size_t length);
