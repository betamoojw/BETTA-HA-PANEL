/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#pragma once
/* Runtime-selectable public key for storage fault injection, never used in firmware. */
extern const char *test_public_key;
#define CONFIG_APP_LICENSE_PUBLIC_KEY_HEX test_public_key
