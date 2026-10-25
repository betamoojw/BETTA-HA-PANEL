/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
#include "api/http_socket_budget.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    for (int total = 16; total <= 64; total++) {
        int clients = http_socket_client_budget(total);
        /* Listener/control plus every reserved integration can coexist with
         * a full HTTP pool; LRU fires before accept exhausts global sockets. */
        assert(clients >= 4 && clients <= 12);
        assert(clients + 3 + 9 <= total);
    }
    assert(http_socket_client_budget(16) == 4);
    assert(http_socket_client_budget(32) == 12);
    assert(http_socket_client_budget(8) == 0);
    puts("HTTP socket budgets: passed (legacy and current configurations)");
}
