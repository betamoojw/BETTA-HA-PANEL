/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Identity and storage path of the pinned ESP-SR custom wake resource.
 *
 * The ESP-SR 2.4.7 mn7_cn resource is packed by prepare_custom_wake_model.py.
 * The upload store verifies both the size and digest before exposing the immutable path.
 */
#pragma once
#define XIAOZHI_CUSTOM_BYTES  2671681U
#define XIAOZHI_CUSTOM_SHA256 "2edad3a5633b58891c8e4275d41ef374d91ac419f444d56b3e91eaaa66fa63dd"
#define XIAOZHI_CUSTOM_PATH   "/littlefs/xiaozhi-mn7-2edad3a5633b.bin"
