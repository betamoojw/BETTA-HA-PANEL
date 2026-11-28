/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Combine verified JPEG bands into one JPEG image with restart markers.
 *
 * Retain the first band's coding tables and insert restart markers between
 * later entropy payloads. Only the complete frame gets an end-of-image marker.
 */
#include "api/screenshot_jpeg_join.h"
#include <string.h>

typedef struct
{
    size_t scan, sof;
    uint16_t width, height;
} jpeg_header_t;

static unsigned be16(const uint8_t *p)
{
    return (unsigned) p[0] * 256U + p[1];
}

static void put16(uint8_t *p, unsigned n)
{
    p[0] = (uint8_t) (n >> 8);
    p[1] = (uint8_t) n;
}

/* Parse only the restricted format emitted by the P4 driver. Reject new marker
 * forms rather than silently assemble an incompatible or incomplete image. */
static bool header(const uint8_t *p, size_t n, jpeg_header_t *h)
{
    if (!p || n < 4 || p[0] != 255 || p[1] != 0xd8)
        return false;
    bool sof = false, dqt = false, dht = false;
    for (size_t at = 2; at + 4 <= n;)
    {
        if (p[at] != 255)
            return false;
        unsigned marker = p[at + 1], len = be16(p + at + 2);
        if (len < 2 || len > n - at - 2)
            return false;
        const uint8_t *v = p + at + 4;
        if (marker == 0xc0)
        {
            if (sof || len != 17 || v[0] != 8 || v[5] != 3 || v[6] != 1 || v[9] != 2 || v[12] != 3 || v[7] != 0x11 || v[10] != 0x11 ||
                v[13] != 0x11)
                return false;
            h->sof = at;
            h->height = be16(v + 1);
            h->width = be16(v + 3);
            sof = true;
        }
        else if (marker == 0xdb)
            dqt = true;
        else if (marker == 0xc4)
            dht = true;
        else if (marker == 0xda)
        {
            if (!sof || !dqt || !dht || len != 12 || v[0] != 3 || v[1] != 1 || v[3] != 2 || v[5] != 3 || v[7] != 0 || v[8] != 63 ||
                v[9] != 0)
                return false;
            h->scan = at + 2 + len;
            return h->width && h->height;
        }
        else if (marker != 0xfe && !(marker >= 0xe0 && marker <= 0xef))
            return false;
        at += 2 + len;
    }
    return false;
}

static bool coding_marker(unsigned marker)
{
    return marker == 0xdb || marker == 0xc4 || marker == 0xc0 || marker == 0xda;
}

/* Compare exact tables, component order and scan selectors, not a hash. The
 * joined header has one extra DRI and the final height in SOF0. */
static bool same_coding(const uint8_t *a, size_t an, const uint8_t *b, size_t bn)
{
    size_t x = 2, y = 2;
    while (x + 4 <= an && y + 4 <= bn)
    {
        unsigned am = a[x + 1], bm = b[y + 1];
        unsigned al = be16(a + x + 2), bl = be16(b + y + 2);
        if (!coding_marker(am))
        {
            x += al + 2;
            continue;
        }
        if (!coding_marker(bm))
        {
            y += bl + 2;
            continue;
        }
        if (am != bm || al != bl)
            return false;
        for (unsigned k = 4; k < al + 2; k++)
        {
            if (am == 0xc0 && (k == 5 || k == 6))
                continue;
            if (a[x + k] != b[y + k])
                return false;
        }
        if (am == 0xda)
            return true;
        x += al + 2;
        y += bl + 2;
    }
    return false;
}

bool screenshot_jpeg_join_init(screenshot_jpeg_join_t *j, uint8_t *out, size_t cap, uint32_t w, uint32_t h, uint32_t band_rows)
{
    if (!j || !out || !w || !h || w > 65535 || h > 65535 || !band_rows || band_rows > 65535 || band_rows % 8 ||
        ((w + 7) / 8) * (band_rows / 8) > 65535)
        return false;
    *j = (screenshot_jpeg_join_t) {.data = out, .capacity = cap, .width = w, .height = h, .band_rows = band_rows};
    return true;
}

bool screenshot_jpeg_join_append(screenshot_jpeg_join_t *j, const uint8_t *p, size_t n)
{
    jpeg_header_t h = {0};
    if (!j || j->rows >= j->height || !header(p, n, &h))
        return false;
    unsigned rows = j->height - j->rows;
    if (rows > j->band_rows)
        rows = j->band_rows;
    if (h.width != j->width || h.height != rows || h.scan + 2 > n)
        return false;
    if (j->bands && !same_coding(j->data, j->header_bytes, p, h.scan))
        return false;
    /* The entropy payload may contain FF00 escapes, but no embedded restart,
     * extra scan or truncated marker. EOI must terminate this entire band. */
    size_t end = h.scan;
    for (; end + 1 < n; end++)
    {
        if (p[end] != 255)
            continue;
        if (p[end + 1] == 0)
        {
            end++;
            continue;
        }
        if (p[end + 1] == 0xd9 && end + 2 == n)
            break;
        return false;
    }
    if (end + 2 != n || p[end] != 255 || p[end + 1] != 0xd9 || end == h.scan)
        return false;
    bool last = j->rows + rows == j->height;
    size_t extra = end - h.scan + (j->bands ? 2 : h.scan + 6) + (last ? 2 : 0);
    if (j->used > j->capacity || extra > j->capacity - j->used)
        return false;
    if (!j->bands)
    {
        /* Insert DRI immediately before SOS, leaving one canonical JPEG header. */
        size_t sos = h.scan - 14;
        memcpy(j->data, p, sos);
        uint8_t dri[] = {255, 0xdd, 0, 4, 0, 0};
        put16(dri + 4, ((j->width + 7U) / 8U) * (j->band_rows / 8U));
        memcpy(j->data + sos, dri, sizeof(dri));
        memcpy(j->data + sos + 6, p + sos, h.scan - sos);
        put16(j->data + h.sof + 5, j->height);
        j->header_bytes = j->used = h.scan + 6;
    }
    else
    {
        j->data[j->used++] = 255;
        j->data[j->used++] = (uint8_t) (0xd0 + ((j->bands - 1) & 7));
    }
    memcpy(j->data + j->used, p + h.scan, end - h.scan);
    j->used += end - h.scan;
    if (last)
    {
        j->data[j->used++] = 255;
        j->data[j->used++] = 0xd9;
    }
    j->rows += rows;
    j->bands++;
    return true;
}
