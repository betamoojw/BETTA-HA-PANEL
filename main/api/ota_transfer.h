/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Shared OTA transfer state and bounded request body reads.
 */
#pragma once
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "util/sha256_soft.h"
#define OTA_APP_DESC_OFFSET  (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)) /**< App descriptor offset in bytes. */
#define OTA_HEADER_CHECK_LEN (OTA_APP_DESC_OFFSET + sizeof(esp_app_desc_t)) /**< Complete prefix required before the first write. */

/** @brief Per-install stream state owned by the upload handler or URL worker. */
typedef struct
{
    const esp_partition_t *partition;         /**< Borrowed descriptor for the target slot. */
    esp_ota_handle_t handle;                  /**< Open OTA writer while begun is true. */
    bool begun;                               /**< The writer must still be finalized or aborted by its owner. */
    bool header_checked;                      /**< The buffered app header passed its compatibility checks. */
    uint8_t header_buf[OTA_HEADER_CHECK_LEN]; /**< Prefix retained until validation permits the first flash write. */
    size_t header_len;                        /**< Bytes collected in the prefix. */
    size_t written;                           /**< Stream bytes successfully passed to the OTA writer. */
    size_t total;                             /**< Expected total bytes, or zero when no length is available. */
    char version[32];                         /**< Version copied from the validated application description. */
    char project_name[32];                    /**< Project identity copied from the validated application description. */
    char error[160];                          /**< NUL-terminated diagnostic for the caller's response. */
    bool verify;                              /**< Compare the stream digest before changing the boot partition. */
    char expected_sha256[65];                 /**< Expected hex digest and NUL terminator, when verification is enabled. */
    sha256_soft_t sha;                        /**< Software SHA-256 after a hardware digest mismatch on this stream (2026-10-02). */
} api_ota_stream_t;

/** @brief Copy a bounded error into stream state without ending the OTA handle. */
void ota_stream_set_error(api_ota_stream_t *stream, const char *message);

/** @brief Finalize the image under the flash gate and check any expected digest before boot selection.
 *
 * The caller still owns cleanup when a failure leaves begun set. A digest
 * mismatch after finalization leaves the writer closed and the boot slot unchanged.
 */
esp_err_t ota_stream_finish(api_ota_stream_t *stream);

/** @brief Disarm digest checking and abort an open writer without selecting a boot slot. */
void ota_stream_abort(api_ota_stream_t *stream);
/** @brief Read a bounded request body with both idle and total receive deadlines.
 * @param req Borrowed request carrying the body.
 * @param max_len Maximum body length in bytes, excluding the added NUL.
 * @return NUL-terminated body allocated for the caller to free, or NULL on failure.
 */
char *ota_read_request_body(httpd_req_t *req, size_t max_len);
