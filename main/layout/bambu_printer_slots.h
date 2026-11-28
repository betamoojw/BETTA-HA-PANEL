/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md
 *
 * The entity bindings of the Bambu printer widget (bambu_printer): their JSON keys, the domain each accepts and
 * the generic widget slot each is stored in. One table for the layout validator, the subscription collector, the
 * firmware parser and the desktop simulator.
 */
/** @file
 * @brief Shared Bambu printer binding metadata and slot extraction.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "app_config.h"
#include "cJSON.h"

/* extra_entity_id[] of the Bambu printer widget; the primary entity is the print status, the nozzle and bed
 * temperatures use secondary_entity_id and tertiary_entity_id. */
#define UI_BAMBU_EXTRA_PROGRESS  0
#define UI_BAMBU_EXTRA_REMAINING 1
#define UI_BAMBU_EXTRA_PAUSE     2
#define UI_BAMBU_EXTRA_RESUME    3
#define UI_BAMBU_EXTRA_STOP      4
#define UI_BAMBU_EXTRA_STAGE     5
#define UI_BAMBU_EXTRA_JOB       6
#define UI_BAMBU_EXTRA_CAMERA    7
#define BAMBU_EXTRA_COUNT        8 /* extra_entity_id[] entries it needs; ui_widget_factory.h asserts the capacity */

#define BAMBU_SLOT_PRIMARY   (-1)
#define BAMBU_SLOT_SECONDARY (-2)
#define BAMBU_SLOT_TERTIARY  (-3)

/** @brief One binding recognized by the layout validator and printer discovery. */
typedef struct
{
    const char *key;             /**< Persisted widget JSON key. */
    int slot;                    /**< Negative primary/secondary/tertiary selector, or nonnegative extra slot. */
    const char *domain;          /**< Accepted Home Assistant entity domain. */
    bool action;                 /**< The binding identifies a button the widget can press. */
    const char *translation_key; /**< Integration registry key used to find the corresponding entity. */
} bambu_printer_binding_t;

/** @brief Shared mapping of printer settings to runtime slots. */
extern const bambu_printer_binding_t k_bambu_printer_bindings[];
/** @brief Number of binding descriptors, including the primary slots. */
extern const size_t k_bambu_printer_binding_count;

/** @brief Copy configured IDs into caller-owned slots without truncation.
 *
 * Missing, non-string, and oversized values leave their outputs unchanged.
 * Any output pointer can be NULL to skip its slot.
 *
 * @param widget Borrowed widget JSON.
 * @param[out] primary Print status slot, APP_MAX_ENTITY_ID_LEN bytes if supplied.
 * @param[out] secondary Nozzle temperature slot of the same capacity.
 * @param[out] tertiary Bed temperature slot of the same capacity.
 * @param[out] extra Array of optional binding slots of the same capacity.
 * @param extra_count Number of writable entries in extra.
 */
void bambu_printer_slots_from_json(const cJSON *widget, char *primary, char *secondary, char *tertiary,
                                   char (*extra)[APP_MAX_ENTITY_ID_LEN], size_t extra_count);
