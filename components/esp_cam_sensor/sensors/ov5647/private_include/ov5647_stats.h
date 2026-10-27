/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 andre68723
 * OV5647 automatic exposure/gain readback for BETTA ambient sensing.
 */
#pragma once

#include <stdint.h>

/* Exposure is a 20-bit value in sixteenths of a line; gain is Q6.4.
 * Discard reserved bits. Keep fractions until the caller chooses its units. */
static inline float ov5647_exposure_lines(uint8_t hi, uint8_t mid, uint8_t lo)
{
    return (float)(((uint32_t)(hi & 0x0f) << 16) | ((uint32_t)mid << 8) | lo) / 16.0f;
}

static inline float ov5647_gain_factor(uint8_t hi, uint8_t lo)
{
    return (float)(((uint32_t)(hi & 0x03) << 8) | lo) / 16.0f;
}
