/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "camera/camera_image.h"

#include <math.h>
#include <string.h>

static int clamp(int x)
{
    return x < 0 ? 0 : x > 255 ? 255 : x;
}
static void rgb(uint16_t p, int *r, int *g, int *b)
{
    *r = ((p >> 11) & 31) * 255 / 31;
    *g = ((p >> 5) & 63) * 255 / 63;
    *b = (p & 31) * 255 / 31;
}
static int tone(int v, const camera_image_config_t *c)
{
    v = clamp((v - 128) * c->contrast / 128 + 128 + c->brightness);
    int low = 255 - v;
    return clamp(v + c->tone_shadows * low * low / (100 * 255) + c->tone_highlights * v * v / (100 * 255));
}
bool camera_image_size(int w, int h, int resolution, int rotation, int *ow, int *oh)
{
    if (!ow || !oh || w < 2 || h < 2 || w > 2048 || h > 2048 || resolution < 0 || resolution > 1 ||
        rotation < 0 || rotation > 270 || rotation % 90)
        return false;
    int step = resolution ? 2 : 1;
    *ow = (rotation % 180 ? h : w) / step;
    *oh = (rotation % 180 ? w : h) / step;
    return true;
}
bool camera_image_render(const uint16_t *src, int w, int h, uint16_t *dst, size_t capacity, const camera_config_t *cfg, int rotation)
{
    int ow, oh;
    if (!src || !dst || !cfg || !camera_image_size(w, h, cfg->resolution, rotation, &ow, &oh))
        return false;
    int step = cfg->resolution ? 2 : 1, sw = w / step, sh = h / step;
    if (capacity < (size_t) ow * oh * 2)
        return false;
    const camera_image_config_t *c = &cfg->image;
    bool color = c->brightness || c->contrast != 128 || c->saturation != 128 || c->hue % 360 || c->wb_red != 100 || c->wb_blue != 100 ||
                 c->tone_shadows || c->tone_highlights;
    bool spatial = c->sharpen != 100 || c->denoise != 100;
    int cs = (int) lroundf(cosf(c->hue * 0.01745329252f) * 1024), sn = (int) lroundf(sinf(c->hue * 0.01745329252f) * 1024);
    for (int y = 0; y < oh; y++)
        for (int x = 0; x < ow; x++)
        {
            int sx = x, sy = y;
            switch (rotation) {
            case 90: sx = y; sy = sh - 1 - x; break;
            case 180: sx = sw - 1 - x; sy = sh - 1 - y; break;
            case 270: sx = sw - 1 - y; sy = x; break;
            }
            sx = (cfg->hflip ? sw - 1 - sx : sx) * step;
            sy = (cfg->vflip ? sh - 1 - sy : sy) * step;
            uint16_t p = src[(size_t) sy * w + sx];
            if (!color && !spatial)
            {
                dst[(size_t) y * ow + x] = p;
                continue;
            }
            int r, g, b;
            rgb(p, &r, &g, &b);
            if (spatial)
            {
                int ar = 0, ag = 0, ab = 0, n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        int xx = sx + dx, yy = sy + dy;
                        if (xx < 0 || xx >= w || yy < 0 || yy >= h)
                            continue;
                        int rr, gg, bb;
                        rgb(src[(size_t) yy * w + xx], &rr, &gg, &bb);
                        ar += rr;
                        ag += gg;
                        ab += bb;
                        n++;
                    }
                int amount = (c->sharpen - 100) - (c->denoise - 100);
                r = clamp(r + (r - ar / n) * amount / 100);
                g = clamp(g + (g - ag / n) * amount / 100);
                b = clamp(b + (b - ab / n) * amount / 100);
            }
            r = clamp(r * c->wb_red / 100);
            b = clamp(b * c->wb_blue / 100);
            int l = (77 * r + 150 * g + 29 * b) / 256, u = b - l, v = r - l;
            int uu = (u * cs - v * sn) / 1024 * c->saturation / 128, vv = (u * sn + v * cs) / 1024 * c->saturation / 128;
            r = tone(l + vv, c);
            b = tone(l + uu, c);
            g = tone(l - (77 * vv + 29 * uu) / 150, c);
            dst[(size_t) y * ow + x] = (uint16_t) (((r * 31 / 255) << 11) | ((g * 63 / 255) << 5) | (b * 31 / 255));
        }
    return true;
}
