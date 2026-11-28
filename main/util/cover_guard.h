/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 */
/** @file
 * @brief Trust and bounds checks for image URLs and bytes from Home Assistant attributes.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Check an exact, case-sensitive prefix ending at a configured origin boundary.
 * @param url Full candidate URL.
 * @param base_url Configured Home Assistant origin without a trailing slash.
 * @return True when the base matches and is followed by the end, `/`, `?`, or `#`.
 */
bool cover_url_on_origin(const char *url, const char *base_url);

/** @brief Credentials the image fetcher may use for the resulting URL. */
typedef enum
{
    COVER_HTTP_ANONYMOUS,  /**< Do not attach Home Assistant credentials or overrides. */
    COVER_HTTP_SIGNED_URL, /**< Keep the URL's own token instead of adding a Bearer header. */
    COVER_HTTP_BEARER,     /**< Use the configured Home Assistant access token. */
} cover_http_auth_t;

/** @return True for a camera proxy path, relative or absolute. This does not check origin trust. */
bool cover_url_is_camera_proxy(const char *url);
/** @brief Select image request authentication for a writable full URL.
 *
 * Same-origin camera requests discard query tokens in place. Other signed
 * URLs and foreign URLs are unchanged. Only the configured origin may use
 * the Home Assistant token, Host override or pinned TLS name.
 *
 * @warning A Bearer request requires a nonempty credential and disabled
 *          redirects; do not make an anonymous fallback request.
 */
cover_http_auth_t cover_prepare_image_auth(char *url, const char *base_url);

/** @return True if the full PNG chunk, from the length field at pos through its CRC, fits in len bytes.
 *
 * Compare the data length with remaining capacity rather than adding to it,
 * so a large chunk length cannot wrap on 32-bit targets.
 */
bool cover_png_chunk_fits(size_t pos, size_t len, uint32_t chunk_len);

/** @return True for nonzero dimensions whose pixel product does not exceed max_pixels. */
bool cover_image_size_ok(uint32_t w, uint32_t h, uint32_t max_pixels);
