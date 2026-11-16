/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
static void diagnostic_cases(void)
{
    for(int call=1;call<=2;call++) {
        reset();fail_lock_at=call;
        CHECK(invoke(0)==ESP_OK && !strcmp(status,"503 Service Unavailable") && !render_calls);
        CHECK(unlock_calls==call-1 && strstr(capture_log,"stage=display.lock") && memory_samples==1);clean();
    }
    reset();display_missing=true; CHECK(invoke(0)==ESP_OK && !strcmp(status,"500 Internal Server Error"));clean();
    const int dimensions[]={0,-1,4097};
    for(unsigned i=0;i<3;i++) { reset();display_w=dimensions[i];CHECK(invoke(0)==ESP_OK && !strcmp(status,"500 Internal Server Error"));clean(); }
    reset();change_geometry=true;CHECK(invoke(0)==ESP_OK && !strcmp(status,"500 Internal Server Error") && !render_calls);clean();
    reset();fail_alloc_at=2;CHECK(invoke(0)==ESP_OK);CHECK(strstr(capture_log,"stage=frame.allocate") && sampled_bytes==1048576);clean();
    reset();fail_alloc_at=3;CHECK(invoke(0)==ESP_OK);CHECK(strstr(capture_log,"stage=top.allocate") && sampled_bytes==1048594);clean();
    reset();fail_render_at=1;CHECK(invoke(0)==ESP_OK);CHECK(strstr(capture_log,"stage=top.render") && strstr(capture_log,"hold_us=400"));clean();
    reset();fail_render_at=2;CHECK(invoke(0)==ESP_OK);CHECK(strstr(capture_log,"stage=screen.render") && strstr(capture_log,"hold_us=700"));clean();
    reset();CHECK(invoke(0)==ESP_OK);
    CHECK(strstr(capture_log,"capture=ok") && strstr(capture_log,"wait_us=5000") && strstr(capture_log,"hold_us=700") && diag_logs==1);clean();
    reset();fail_lock_at=1;fail_send_at=1;CHECK(invoke(0)==ESP_FAIL);clean();
    reset();display_w=1280;display_h=800;total_limit=2000000;CHECK(invoke(0)==ESP_OK && !strcmp(status,"500 Internal Server Error"));clean();
}
