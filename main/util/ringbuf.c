/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 Cpt_Kirk
 */
/** @file
 * @brief Byte FIFO over caller-owned storage without allocation or locking.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Ring state with wrapped read/write indices and occupancy in bytes. */
typedef struct
{
    uint8_t *buf;
    size_t capacity;
    size_t read_idx;
    size_t write_idx;
    size_t len;
} ringbuf_t;

/** @brief Append a byte without evicting unread data.
 * @return False without advancing the ring when it is full or missing storage.
 */
bool ringbuf_write_byte(ringbuf_t *rb, uint8_t value)
{
    if (rb == NULL || rb->buf == NULL || rb->len == rb->capacity)
    {
        return false;
    }
    rb->buf[rb->write_idx] = value;
    rb->write_idx = (rb->write_idx + 1U) % rb->capacity;
    rb->len++;
    return true;
}

/** @brief Consume the oldest byte if the destination and unread data are available. */
bool ringbuf_read_byte(ringbuf_t *rb, uint8_t *out_value)
{
    if (rb == NULL || rb->buf == NULL || out_value == NULL || rb->len == 0U)
    {
        return false;
    }
    *out_value = rb->buf[rb->read_idx];
    rb->read_idx = (rb->read_idx + 1U) % rb->capacity;
    rb->len--;
    return true;
}
