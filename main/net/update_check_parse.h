/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * Pure part of the daily firmware update check (net/update_check.c): reads a GitHub release API
 * response and decides whether it holds a newer build for this panel. No ESP-IDF dependencies, so
 * the host tests cover it.
 */
/** @file
 * @brief Host-testable release parsing, variant mapping and digest comparison.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Minimum asset age relative to build time for a version tie, in seconds.
 *
 * A running firmware that reports a plain version (v0.9.0) cannot say which candidate or release it
 * is: a release on the same core (v0.9.0-rc3, v0.9.0, or a re-upload) then counts as newer only when
 * its asset was uploaded this long after the firmware was built. A firmware that reports its
 * pre-release (v0.9.0-rc2, set in CMakeLists.txt or by BETTA_VERSION) is compared as a whole: rc3 and
 * v0.9.0 are newer at once, the same version only by this margin (a re-upload). The margin also covers
 * a build time read in the panel's time zone that differs from the build machine's.
 */
#define UPDATE_CHECK_SAME_VERSION_MARGIN_S (12 * 3600)

/** @brief Owned copy of the selected release metadata; no fields borrow from the parsed JSON. */
typedef struct
{
    bool found;            /**< A release carries an OTA asset for this variant. */
    bool newer;            /**< The selected build is newer than the running firmware. */
    bool prerelease;       /**< Release channel reported by the selected entry. */
    char tag[32];          /**< Selected release tag. */
    char asset_url[256];   /**< Download URL of the variant's OTA binary. */
    char asset_sha256[65]; /**< Lowercase asset digest, or empty when no valid digest was supplied. */
    int64_t published;     /**< Asset update time in Unix seconds, or zero when unknown. */
} update_check_release_t;

/** @return Borrowed release suffix for the exact ESP app project name, or NULL if unknown. */
const char *update_check_variant_for_project(const char *project_name);

/** @brief Compare two release tags, with or without the leading v.
 *
 * A pre-release sorts before its release; "rc10" sorts after "rc2".
 * Unparsable tags compare as 0.0.0. With core_only, pre-release parts are ignored.
 * @return Negative, zero or positive as a sorts before, equal to or after b.
 */
int update_check_compare_versions(const char *a, const char *b, bool core_only);

/** @brief Parse a Z-suffixed release timestamp into Unix seconds. */
bool update_check_parse_iso8601_utc(const char *text, int64_t *out_epoch);

/** @return True when all 64 case-insensitive hex digits encode the supplied 32-byte digest. */
bool update_check_sha256_matches(const char *hex, const uint8_t digest[32]);

/** @brief Select the highest eligible release carrying an OTA asset for this variant.
 * @param json Borrowed release object or array returned by the release API.
 * @param json_len Available JSON bytes.
 * @param variant Exact release asset variant suffix.
 * @param include_prereleases Whether to consider pre-releases in addition to releases; drafts are skipped.
 * @param running_version Non-NULL version string for comparison.
 * @param build_epoch Running image build time in Unix seconds, or zero if unknown.
 * @param[out] out Caller-owned metadata and comparison result.
 * @return True for a readable response, including when no matching release was found.
 */
bool update_check_parse_releases(const char *json, size_t json_len, const char *variant, bool include_prereleases,
                                 const char *running_version, int64_t build_epoch, update_check_release_t *out);
