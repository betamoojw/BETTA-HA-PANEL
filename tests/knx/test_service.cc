// SPDX-License-Identifier: LicenseRef-FNCL-1.1
// Copyright (c) 2026 andre68723
// Execute the production worker deterministically; no sockets, threads or bus.
#include "knx.h"
#include "knx_config.h"
#include "esp_knx_ip/esp_knx_ip.h"
#include "freertos/queue.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>
#include <functional>

bool test_online=true, test_storage_failure=false;
uint32_t test_ip=1;
int64_t test_now=1000000;
uint8_t test_enabled=0;
void (*test_worker)(void*);
static size_t command_size;
static std::deque<std::vector<unsigned char>> queue;
static std::function<void()> iteration;
static std::string persisted;
static unsigned starts,stops,sends;
static esp_knx_ip_telegram_callback_t callback;
static knx_address_t source;
static bool fail_start;
struct Done {};
QueueHandle_t xQueueCreate(unsigned,size_t size) { command_size=size;return (void*)1; }
void vQueueDelete(QueueHandle_t) {}
int xQueueSend(QueueHandle_t,const void* data,unsigned) {
    if(queue.size()==8) return 0;
    const auto* b=static_cast<const unsigned char*>(data);queue.emplace_back(b,b+command_size);return pdTRUE;
}
int xQueueReceive(QueueHandle_t,void* out,unsigned) {
    test_now+=20000000; iteration();
    if(queue.empty()) return 0;
    memcpy(out,queue.front().data(),command_size);queue.pop_front();return pdTRUE;
}
extern "C" bool wifi_mgr_is_connected(void) { return test_online; }
extern "C" bool flash_gate_enter(uint32_t) { return !test_storage_failure; }
extern "C" void flash_gate_leave(void) {}
bool KnxLoadConfiguration(std::string& text,std::string&) { text=persisted;return true; }
bool KnxSaveConfiguration(const std::string& text,std::string& error) {
    if(test_storage_failure) {error="test write failed";return false;} persisted=text;return true;
}
extern "C" esp_err_t esp_knx_ip_create(const esp_knx_ip_config_t* cfg,esp_knx_ip_handle_t* h) {
    assert(cfg->port==3672 && !strcmp(cfg->multicast_address,"224.0.23.13"));
    assert(!cfg->load_physical_address_from_nvs);
    source=cfg->physical_address;*h=reinterpret_cast<esp_knx_ip_handle_t>(1);return ESP_OK;
}
extern "C" esp_err_t esp_knx_ip_register_callback(esp_knx_ip_handle_t,knx_address_t,esp_knx_ip_telegram_callback_t c,void*) {callback=c;return ESP_OK;}
extern "C" esp_err_t esp_knx_ip_start(esp_knx_ip_handle_t) {starts++;return fail_start?ESP_FAIL:ESP_OK;}
extern "C" esp_err_t esp_knx_ip_stop(esp_knx_ip_handle_t) {stops++;return ESP_OK;}
extern "C" esp_err_t esp_knx_ip_destroy(esp_knx_ip_handle_t) {return ESP_OK;}
extern "C" esp_err_t esp_knx_ip_send(esp_knx_ip_handle_t,knx_address_t address,knx_command_t command,const uint8_t* data,size_t n) {
    assert(address==knx_group_address(1,0,1) && command==KNX_COMMAND_WRITE && n==1 && data[0]==1);
    sends++;return ESP_OK;
}
static void status_is(const char* expected) {
    cJSON* s=knx_status();assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(s,"state")),expected));cJSON_Delete(s);
}
static bool cached() {
    cJSON* o=knx_object("lamp");assert(o);bool v=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o,"valid"));cJSON_Delete(o);return v;
}
int main() {
    persisted=kKnxDefaultConfiguration;
    persisted.replace(persisted.find("3671"),4,"3672");
    persisted.replace(persisted.find("224.0.23.12"),11,"224.0.23.13");
    persisted.replace(persisted.find("[]"),2,R"([{"id":"lamp","name":"Light","group_address":"1/0/1","datapoint_type":"DPT-1.001","readable":true,"writable":true}])");
    knx_init();assert(test_worker);char error[200];
    assert(!knx_command("lamp",nullptr,"on",true,error,sizeof(error)));assert(starts==0);
    assert(knx_enable(true,error,sizeof(error)));
    int step=0;
    iteration=[&] {
        switch(step++) {
        case 0: {
            status_is("running");assert(!cached());
            assert(!knx_command("missing",nullptr,"on",true,error,sizeof(error)));
            assert(!knx_command("lamp",nullptr,"invalid",true,error,sizeof(error)));
            assert(knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            knx_telegram_t t{};t.source=source;t.destination=knx_group_address(1,0,1);
            t.command=KNX_COMMAND_RESPONSE;t.data_length=1;t.data[0]=1;
            callback(&t,nullptr);assert(!cached()); // ignore multicast loopback
            t.source=0x1102;callback(&t,nullptr);assert(cached());
            std::string before=persisted;
            assert(!knx_configure("[]",error,sizeof(error)));assert(persisted==before);
            test_storage_failure=true;
            assert(!knx_configure(before.c_str(),error,sizeof(error)));assert(persisted==before);
            assert(!knx_enable(false,error,sizeof(error)));status_is("running");
            test_storage_failure=false;break;
        }
        case 1:
            assert(sends==1 && cached());
            assert(knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            assert(knx_enable(false,error,sizeof(error)));break;
        case 2:
            status_is("disabled");assert(stops==1 && sends==1); // queued command revoked
            assert(!knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            assert(knx_enable(true,error,sizeof(error)));break;
        case 3:
            status_is("running");assert(starts==2);test_online=false;break;
        case 4:
            status_is("waiting_for_network");assert(stops==2);
            test_online=true;test_ip=2;break;
        case 5: {
            status_is("running");assert(starts==3);
            assert(knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            std::string updated=persisted;updated.replace(updated.find("\"writable\":true"),15,"\"writable\":false");
            assert(knx_configure(updated.c_str(),error,sizeof(error)));break;
        }
        case 6:
            status_is("running");assert(starts==4 && stops==3 && sends==1 && !cached());
            assert(!knx_command("lamp",nullptr,"on",true,error,sizeof(error)));
            test_ip=3;fail_start=true;break;
        case 7:
            status_is("error");fail_start=false;break;
        case 8:
            status_is("running");
            assert(knx_enable(false,error,sizeof(error)));break;
        default:
            status_is("disabled");assert(sends==1);throw Done{};
        }
    };
    try {test_worker(nullptr);} catch(const Done&) {}
    puts("KNX worker: enable/disable, permissions, generation revocation, feedback, storage failure, reconnect and retry passed");
}
