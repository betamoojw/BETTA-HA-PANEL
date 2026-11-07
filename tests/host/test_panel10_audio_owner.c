/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "drivers/audio_out.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* LCD-X has a persistent assistant audio owner. Chimes must not advertise
     * or open a second I2S channel, even when a valid tone is requested. */
    const int16_t tone[] = {0, 100, -100, 0};
    assert(!audio_out_available());
    assert(audio_out_play(tone, 4, 60, NULL) == ESP_ERR_NOT_SUPPORTED);
    puts("Panel10 audio ownership: independent chime path disabled");
    return 0;
}
