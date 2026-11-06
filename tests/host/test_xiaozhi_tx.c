/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xiaozhi/xiaozhi_tx.h"
#include "esp_heap_caps.h"
static unsigned allocated, calls;
static bool oom;
static int result;
static void tracked_free(void *p) { if (p) { assert(allocated); --allocated; free(p); } }
void *heap_caps_malloc(size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (oom) return NULL;
    void *p = malloc(size); assert(p); ++allocated; return p;
}
#define free tracked_free
#include "../../main/xiaozhi/xiaozhi_tx.c"
#undef free
static int send_mock(void *context, const void *data, int length, bool binary)
{
    assert(context && !memcmp(data, context, length));
    assert(binary == (length == 4)); ++calls;
    return result;
}
int main(void)
{
    xiaozhi_tx_t queue = {0}; bool binary;
    assert(xiaozhi_tx_pump(&queue, 0, send_mock, NULL, &binary) == XZ_TX_IDLE && !binary);
    char audio[] = "opus";
    assert(xiaozhi_tx_enqueue(&queue, audio, 4, true, 100));
    audio[0] = 'X'; /* Producer storage is reused before retry. */
    assert(xiaozhi_tx_enqueue(&queue, "listen", 6, false, 200));
    result = XIAOZHI_TX_LOCK_BUSY;
    for (int i = 0; i < 50; ++i)
        assert(xiaozhi_tx_pump(&queue, 1000 + i * 20000, send_mock, "opus", &binary) == XZ_TX_BUSY);
    assert(queue.count == 2 && allocated == 2 && !binary);
    result = 4;
    assert(xiaozhi_tx_pump(&queue, 2000000, send_mock, "opus", &binary) == XZ_TX_SENT && binary);
    result = 6;
    assert(xiaozhi_tx_pump(&queue, 2000000, send_mock, "listen", &binary) == XZ_TX_SENT && !binary);
    assert(!allocated && !queue.count && !queue.bytes && calls == 52);
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < XIAOZHI_TX_SLOTS; ++i) assert(xiaozhi_tx_enqueue(&queue, "opus", 4, true, 0));
        assert(!xiaozhi_tx_enqueue(&queue, "opus", 4, true, 0));
        result = 4;
        for (int i = 0; i < XIAOZHI_TX_SLOTS; ++i)
            assert(xiaozhi_tx_pump(&queue, 1, send_mock, "opus", &binary) == XZ_TX_SENT);
    }
    assert(!xiaozhi_tx_enqueue(&queue, NULL, 4, true, 0));
    assert(!xiaozhi_tx_enqueue(&queue, "", 0, false, 0));
    assert(!xiaozhi_tx_enqueue(&queue, "", -1, false, 0));
    assert(!xiaozhi_tx_enqueue(&queue, "", XIAOZHI_TX_MAX_BYTES + 1, false, 0));
    char *large = calloc(1, XIAOZHI_TX_MAX_BYTES); assert(large);
    assert(xiaozhi_tx_enqueue(&queue, large, XIAOZHI_TX_MAX_BYTES, false, 0));
    assert(!xiaozhi_tx_enqueue(&queue, "x", 1, false, 0));
    free(large); xiaozhi_tx_clear(&queue); assert(!allocated);
    oom = true; assert(!xiaozhi_tx_enqueue(&queue, "opus", 4, true, 0)); oom = false;
    assert(!queue.count && !queue.bytes);
    assert(xiaozhi_tx_enqueue(&queue, "opus", 4, true, 100));
    result = XIAOZHI_TX_LOCK_BUSY;
    assert(xiaozhi_tx_pump(&queue, 100 + XIAOZHI_TX_DEADLINE_US - 1, send_mock, "opus", &binary) == XZ_TX_BUSY);
    unsigned before = calls;
    assert(xiaozhi_tx_pump(&queue, 100 + XIAOZHI_TX_DEADLINE_US, send_mock, "opus", &binary) == XZ_TX_EXPIRED);
    assert(calls == before); xiaozhi_tx_clear(&queue);
    for (int i = -1; i < 4; ++i) {
        assert(xiaozhi_tx_enqueue(&queue, "opus", 4, true, 0)); result = i;
        assert(xiaozhi_tx_pump(&queue, 1, send_mock, "opus", &binary) == XZ_TX_FAILED);
        xiaozhi_tx_clear(&queue);
    }
    xiaozhi_tx_clear(&queue); assert(!allocated && !queue.count && !queue.bytes);
    puts("Assistant transmit FIFO, retry, deadline, limits, allocation and cleanup passed");
}
