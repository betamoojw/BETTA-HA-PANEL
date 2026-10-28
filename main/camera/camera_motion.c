/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Motion controls follow the JC reference's feature set; this implementation
 * uses overlapping zone membership and rejects lighting changes globally.
 */
#include "camera/camera_motion.h"

#include <stdlib.h>
#include <string.h>

void camera_motion_reset(camera_motion_state_t *s, int64_t now)
{
    uint32_t count = s->status.trigger_count;
    int64_t last = s->status.last_trigger_ms;
    memset(s, 0, sizeof(*s));
    s->start_ms = now;
    s->active_since_ms = -1;
    s->status.trigger_count = count;
    s->status.last_trigger_ms = last;
}
bool camera_motion_sample(camera_motion_state_t *s, const camera_motion_config_t *c, const uint8_t *grid, int64_t now)
{
    uint32_t count = s->status.trigger_count;
    int64_t last = s->status.last_trigger_ms;
    memset(&s->status, 0, sizeof(s->status));
    s->status.trigger_count = count;
    s->status.last_trigger_ms = last;
    bool valid = s->have_previous && now >= s->previous_ms && now - s->previous_ms <= 4000;
    int total = 0, changed = 0, sum = 0, global_changed = 0, pos = 0, neg = 0;
    int cells[CAMERA_MAX_ZONES] = { 0 }, diff[CAMERA_MAX_ZONES] = { 0 }, changes[CAMERA_MAX_ZONES] = { 0 };
    for (int y = 0; y < CAMERA_MOTION_GRID; y++)
        for (int x = 0; x < CAMERA_MOTION_GRID; x++)
        {
            int i = y * CAMERA_MOTION_GRID + x, d = (int) grid[i] - s->previous[i], ad = abs(d);
            if (ad >= 12)
            {
                global_changed++;
                if (d > 0)
                    pos++;
                else
                    neg++;
            }
            int px = (2 * x + 1) * 100 / (2 * CAMERA_MOTION_GRID), py = (2 * y + 1) * 100 / (2 * CAMERA_MOTION_GRID);
            bool included = c->zone_count == 0;
            for (int z = 0; z < c->zone_count; z++)
            {
                const camera_zone_t *r = &c->zones[z];
                if (px < r->x || px >= r->x + r->w || py < r->y || py >= r->y + r->h)
                    continue;
                included = true;
                cells[z]++;
                diff[z] += ad;
                if (ad >= 12)
                    changes[z]++;
            }
            if (included)
            {
                total++;
                sum += ad;
                if (ad >= 12)
                    changed++;
            }
        }
    memcpy(s->previous, grid, sizeof(s->previous));
    s->have_previous = true;
    s->previous_ms = now;
    s->status.valid = valid;
    if (!valid || now - s->start_ms < c->start_delay_ms)
    {
        s->active_since_ms = -1;
        return false;
    }
    s->status.level = total ? sum / total : 0;
    s->status.changed_pct = total ? 100 * changed / total : 0;
    for (int z = 0; z < c->zone_count; z++)
    {
        s->status.zone_level[z] = cells[z] ? diff[z] / cells[z] : 0;
        s->status.zone_changed_pct[z] = cells[z] ? 100 * changes[z] / cells[z] : 0;
    }
    s->status.ignored_lighting = c->ignore_lighting && global_changed * 100 >= 60 * CAMERA_MOTION_GRID * CAMERA_MOTION_GRID &&
                                 (pos > neg ? pos : neg) * 100 >= global_changed * 90;
    bool active = total && !s->status.ignored_lighting && s->status.level >= c->threshold && s->status.changed_pct >= c->min_area;
    s->status.active = active;
    if (!active)
    {
        s->active_since_ms = -1;
        return false;
    }
    if (s->active_since_ms < 0)
        s->active_since_ms = now;
    if (now - s->active_since_ms < c->min_duration_ms || (count && now - last < c->cooldown_ms))
        return false;
    s->status.trigger_count++;
    s->status.last_trigger_ms = now;
    return true;
}
