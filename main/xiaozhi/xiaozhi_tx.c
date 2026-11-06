/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include "xiaozhi_tx.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>

bool xiaozhi_tx_enqueue(xiaozhi_tx_t *queue, const void *data, int length, bool binary, int64_t now_us)
{
    if (!data || length <= 0 || length > XIAOZHI_TX_MAX_BYTES || queue->count == XIAOZHI_TX_SLOTS ||
        (unsigned)length > XIAOZHI_TX_MAX_BYTES - queue->bytes) return false;
    uint8_t *copy = heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) return false;
    memcpy(copy, data, length);
    queue->items[(queue->head + queue->count) % XIAOZHI_TX_SLOTS] =
        (xiaozhi_tx_packet_t){copy, length, binary, now_us};
    queue->count++; queue->bytes += length;
    return true;
}

xiaozhi_tx_result_t xiaozhi_tx_pump(xiaozhi_tx_t *queue, int64_t now_us,
    xiaozhi_tx_send_fn send, void *context, bool *sent_binary)
{
    *sent_binary = false;
    if (!queue->count) return XZ_TX_IDLE;
    xiaozhi_tx_packet_t *packet = &queue->items[queue->head];
    if (now_us - packet->queued_us >= XIAOZHI_TX_DEADLINE_US) return XZ_TX_EXPIRED;
    int sent = send(context, packet->data, packet->length, packet->binary);
    if (sent == XIAOZHI_TX_LOCK_BUSY) return XZ_TX_BUSY;
    if (sent != packet->length) return XZ_TX_FAILED;
    *sent_binary = packet->binary;
    queue->bytes -= packet->length;
    free(packet->data); memset(packet, 0, sizeof(*packet));
    queue->head = (queue->head + 1) % XIAOZHI_TX_SLOTS;
    queue->count--;
    return XZ_TX_SENT;
}

void xiaozhi_tx_clear(xiaozhi_tx_t *queue)
{
    for (unsigned i = 0; i < XIAOZHI_TX_SLOTS; i++) free(queue->items[i].data);
    memset(queue, 0, sizeof(*queue));
}
