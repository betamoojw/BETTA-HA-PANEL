/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "api/screenshot_jpeg_join.h"
#include <stdio.h>
#include <stdlib.h>

/* Independent decoder/pixel checks are driven by test_screenshot_jpeg_join.py. */
int main(int argc, char **argv)
{
    if (argc < 5) return 2;
    uint8_t *out = malloc(1U << 20), *band = malloc(1U << 20);
    if (!out || !band) return 2;
    screenshot_jpeg_join_t join;
    int result = 1;
    if (!screenshot_jpeg_join_init(&join, out, 1U << 20, (unsigned)atoi(argv[1]),
                                  (unsigned)atoi(argv[2]), 32)) goto done;
    for (int i = 4; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) goto done;
        size_t size = fread(band, 1, 1U << 20, f);
        bool complete = !ferror(f) && fgetc(f) == EOF;
        fclose(f);
        if (!complete || !screenshot_jpeg_join_append(&join, band, size)) goto done;
    }
    if (join.rows != join.height) goto done;
    FILE *f = fopen(argv[3], "wb");
    if (!f) goto done;
    result = fwrite(out, 1, join.used, f) != join.used;
    if (fclose(f)) result = 1;
done:
    free(band); free(out);
    return result;
}
