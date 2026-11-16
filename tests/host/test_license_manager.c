/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include "settings/license_manager.c"
#include "settings/license_warning.h"
/* Storage/lifecycle test uses fake crypto. The separate PSA target verifies real signatures. */
const char *test_public_key = "configured";
static char disk[LICENSE_FILE_MAX], pending[LICENSE_FILE_MAX];
static size_t disk_len, pending_len;
static bool exists, pending_exists, opened;
static esp_err_t open_error, read_error, write_error, commit_error, mac_error;
static int64_t clock_us;
static unsigned writes;
int64_t esp_timer_get_time(void) { return clock_us; }
esp_err_t esp_efuse_mac_get_default(uint8_t *mac) { memset(mac, 1, 6); return mac_error; }
bool license_device_id(const uint8_t mac[6], char out[65]) { (void)mac; memset(out, 'a', 64); out[64] = 0; return true; }
bool license_public_key_valid(const char *key) { return key && !strcmp(key, "configured"); }
int license_key_verify(const char *pub, const char id[65], const void *key, size_t n) {
    (void)pub; (void)id;
    if (n < 200 || n > 202) return 0;
    for (unsigned i = 0; i < 200; ++i) if (((const char *)key)[i] != 'k') return 0;
    return 1;
}
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *h) {
    assert(!strcmp(name, "betta_license")); assert(!opened); (void)mode;
    if (open_error) return open_error;
    opened = true; *h = 1; pending_exists = exists; pending_len = disk_len; memcpy(pending, disk, sizeof(disk)); return ESP_OK;
}
void nvs_close(nvs_handle_t h) { assert(h == 1 && opened); opened = false; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *n) {
    assert(h == 1 && opened && !strcmp(key, "device_key"));
    if (read_error) return read_error;
    if (!exists) return ESP_ERR_NVS_NOT_FOUND;
    if (*n < disk_len) return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(out, disk, disk_len); *n = disk_len; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *in, size_t n) {
    assert(h == 1 && opened && !strcmp(key, "device_key") && n == 200); ++writes;
    if (write_error) return write_error;
    memcpy(pending, in, n); pending_len = n; pending_exists = true; return ESP_OK;
}
esp_err_t nvs_erase_key(nvs_handle_t h, const char *key) {
    assert(h == 1 && opened && !strcmp(key, "device_key"));
    if (write_error) return write_error;
    if (!pending_exists) return ESP_ERR_NVS_NOT_FOUND;
    pending_exists = false; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    assert(h == 1 && opened);
    if (commit_error) return commit_error;
    exists = pending_exists; disk_len = pending_len; memcpy(disk, pending, sizeof(disk)); return ESP_OK;
}
static license_status_t snapshot(void) { license_status_t s; license_manager_status(&s); assert(!opened); return s; }
int main(void) {
    for (int state = LICENSE_MISSING; state <= LICENSE_UNCONFIGURED; ++state) {
        assert(!license_warning_due(state, -1));
        assert(!license_warning_due(state, 0));
        assert(!license_warning_due(state, LICENSE_LCD_DELAY_US - 1));
        assert(license_warning_due(state, LICENSE_LCD_DELAY_US) == (state != LICENSE_VALID));
        assert(license_warning_due(state, INT64_MAX) == (state != LICENSE_VALID));
    }
    license_manager_init(); assert(snapshot().key_state == LICENSE_MISSING); assert(snapshot().trial_seconds == 3600);
    clock_us = LICENSE_LCD_DELAY_US;
    assert(LICENSE_LCD_DELAY_US == INT64_C(300000000));
    assert(snapshot().trial_seconds == 3300 && license_warning_due(snapshot().key_state, clock_us));
    clock_us = 3599999999LL; assert(snapshot().trial_seconds == 1);
    clock_us = 3600000000LL; assert(snapshot().trial_seconds == 0);
    clock_us = INT64_MAX; assert(snapshot().trial_seconds == 0);
    char key[202]; memset(key, 'k', 200); key[200] = '\r'; key[201] = '\n';
    assert(license_manager_import(key, sizeof(key)) == ESP_OK); assert(snapshot().key_state == LICENSE_VALID);
    license_manager_init(); assert(snapshot().key_state == LICENSE_VALID); assert(disk_len == 200);
    key[3] = 0; unsigned before = writes;
    assert(license_manager_import(key, sizeof(key)) == ESP_ERR_INVALID_ARG);
    assert(writes == before && snapshot().key_state == LICENSE_VALID); key[3] = 'k';
    assert(!license_warning_due(snapshot().key_state, clock_us));
    commit_error = ESP_FAIL; assert(license_manager_remove() == ESP_FAIL); assert(snapshot().key_state == LICENSE_ERROR);
    commit_error = 0; license_manager_init(); assert(snapshot().key_state == LICENSE_VALID);
    assert(license_manager_remove() == ESP_OK); assert(snapshot().key_state == LICENSE_MISSING && snapshot().trial_seconds == 0);
    assert(license_manager_remove() == ESP_OK);
    write_error = ESP_FAIL; assert(license_manager_import(key, 200) == ESP_FAIL); assert(snapshot().key_state == LICENSE_ERROR);
    write_error = 0; commit_error = ESP_FAIL; assert(license_manager_import(key, 200) == ESP_FAIL);
    commit_error = 0; license_manager_init(); assert(snapshot().key_state == LICENSE_MISSING);
    assert(license_manager_import(key, 200) == ESP_OK);
    disk_len = 202; license_manager_init(); assert(snapshot().key_state == LICENSE_INVALID);
    disk_len = 200; disk[0] = 'x'; license_manager_init(); assert(snapshot().key_state == LICENSE_INVALID);
    read_error = ESP_FAIL; license_manager_init(); assert(snapshot().key_state == LICENSE_ERROR); read_error = 0;
    open_error = ESP_ERR_NVS_NOT_FOUND; license_manager_init(); assert(snapshot().key_state == LICENSE_MISSING);
    open_error = ESP_FAIL; license_manager_init(); assert(snapshot().key_state == LICENSE_ERROR); open_error = 0;
    mac_error = ESP_FAIL; license_manager_init(); assert(!snapshot().device_id[0]); mac_error = 0;
    test_public_key = ""; license_manager_init(); assert(snapshot().key_state == LICENSE_UNCONFIGURED);
    assert(license_manager_import(key, 200) == ESP_ERR_INVALID_STATE);
    test_public_key = "invalid-or-crypto-error"; license_manager_init(); assert(snapshot().key_state == LICENSE_ERROR);
    test_public_key = "configured"; clock_us = 0; exists = false;
    for (unsigned i = 0; i < 100; ++i) {
        license_manager_init(); assert(license_manager_import(key, 200) == ESP_OK);
        license_manager_init(); assert(snapshot().key_state == LICENSE_VALID);
        assert(license_manager_remove() == ESP_OK);
    }
    puts("license storage/lifecycle: PASS (expiry, 100 reload/import/remove cycles, fault injection)");
}
