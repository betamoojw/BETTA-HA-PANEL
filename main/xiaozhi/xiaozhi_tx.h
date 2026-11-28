/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
/** @file
 * @brief Session worker's bounded WebSocket transmit queue.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define XIAOZHI_TX_SLOTS       8
#define XIAOZHI_TX_MAX_BYTES   65536
#define XIAOZHI_TX_DEADLINE_US 5000000
/* Only this pre-write result is retryable; partial writes are never replayed. */
#define XIAOZHI_TX_LOCK_BUSY (-0x107) /* -ESP_ERR_TIMEOUT */

typedef struct
{
    uint8_t *data;
    int length;
    bool binary;
    int64_t queued_us;
} xiaozhi_tx_packet_t;

typedef struct
{
    xiaozhi_tx_packet_t items[XIAOZHI_TX_SLOTS];
    unsigned head, count, bytes;
} xiaozhi_tx_t;

typedef enum
{
    XZ_TX_IDLE,
    XZ_TX_SENT,
    XZ_TX_BUSY,
    XZ_TX_FAILED,
    XZ_TX_EXPIRED
} xiaozhi_tx_result_t;

typedef int (*xiaozhi_tx_send_fn)(void *context, const void *data, int length, bool binary);

/** @brief Copy the caller's payload into PSRAM for the owning session worker. */
bool xiaozhi_tx_enqueue(xiaozhi_tx_t *queue, const void *data, int length, bool binary, int64_t now_us);
/** @brief Attempt only the queue head, without replaying a partial write.
 * @param[in,out] queue Session-worker-owned transmit queue.
 * @param now_us Monotonic time in microseconds on the same clock used for enqueue.
 * @param send Callback that returns bytes sent or a transport error.
 * @param context Borrowed context passed to the callback.
 * @param[out] sent_binary True only for a successfully sent binary packet.
 * @retval XZ_TX_BUSY No bytes were written; the packet may be retried before its deadline.
 * @retval XZ_TX_FAILED The packet was not fully sent; tear down the connection and clear the queue.
 * @retval XZ_TX_EXPIRED The head reached its deadline; tear down the connection and clear the queue.
 * @retval XZ_TX_SENT The full packet was sent and removed from the queue.
 * @retval XZ_TX_IDLE The queue was empty.
 */
xiaozhi_tx_result_t xiaozhi_tx_pump(xiaozhi_tx_t *queue, int64_t now_us, xiaozhi_tx_send_fn send, void *context, bool *sent_binary);
/** @brief Free all queued payloads and reset the queue for reuse. */
void xiaozhi_tx_clear(xiaozhi_tx_t *queue);
