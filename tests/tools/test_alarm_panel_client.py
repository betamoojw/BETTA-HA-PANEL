# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Exercise the real mailbox/worker with deterministic RTOS and transport doubles."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
STUBS = {
    "app_config.h": "#define APP_MAX_WIDGETS_TOTAL 224\n",
    "esp_err.h": "#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_ERR_INVALID_STATE 1\n#define ESP_ERR_NO_MEM 2\n",
    "esp_heap_caps.h": "#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define heap_caps_calloc(n,s,c) calloc(n,s)\n",
    "esp_timer.h": "#include <stdint.h>\nextern int64_t test_now;\nstatic inline int64_t esp_timer_get_time(void) { return test_now; }\n",
    "freertos/FreeRTOS.h": "#pragma once\n#define pdTRUE 1\n#define pdPASS 1\n#define portMAX_DELAY 0xffffffff\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(p) ((void)(p))\n#define portEXIT_CRITICAL(p) ((void)(p))\n",
    "freertos/queue.h": "#pragma once\n#include <stddef.h>\ntypedef void *QueueHandle_t;\nQueueHandle_t xQueueCreate(unsigned, size_t);\nvoid vQueueDelete(QueueHandle_t);\nint xQueueSend(QueueHandle_t,const void *,unsigned);\nint xQueueReceive(QueueHandle_t,void *,unsigned);\n",
    "freertos/task.h": "int xTaskCreate(void (*)(void *), const char *, unsigned, void *, int, void *);\n",
    "ha/ha_client.h": "#pragma once\n#include <stdbool.h>\n#include \"cJSON.h\"\n#include \"esp_err.h\"\ntypedef void (*ha_client_response_cb_t)(bool,cJSON *,void *);\nesp_err_t ha_client_send_command_with_response(cJSON *,ha_client_response_cb_t,void *);\n",
}
HARNESS = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "ha/ha_alarm_panel_client.c"
int64_t test_now;
static job_t jobs[16];
static unsigned queued, sent;
static bool reject;
static jmp_buf end_worker;
static ha_client_response_cb_t callback;
static void *callback_user;
QueueHandle_t xQueueCreate(unsigned count,size_t size) { assert(count==16 && size==sizeof(job_t)); return jobs; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,int priority,void *task)
{ (void)fn; (void)name; (void)stack; (void)arg; (void)priority; (void)task; return pdPASS; }
int xQueueSend(QueueHandle_t q,const void *job,unsigned ticks)
{ (void)q; (void)ticks; if(queued==16) return 0; jobs[queued++]=*(const job_t *)job; return pdTRUE; }
int xQueueReceive(QueueHandle_t q,void *job,unsigned ticks)
{ (void)q; (void)ticks; if(!queued) longjmp(end_worker,1); *(job_t *)job=jobs[0]; memmove(jobs,jobs+1,--queued*sizeof(*jobs)); return pdTRUE; }
esp_err_t ha_client_send_command_with_response(cJSON *command,ha_client_response_cb_t cb,void *user)
{ ++sent; callback=cb; callback_user=user; cJSON_Delete(command); return reject ? ESP_ERR_INVALID_STATE : ESP_OK; }
static void drain(void) { if(!setjmp(end_worker)) worker(NULL); }
static bool poll(uint32_t owner,bool expected,bool history)
{
    bool ok=false,is_history=false; cJSON *reply=NULL;
    bool ready=ha_alarm_panel_poll(owner,&ok,&is_history,&reply);
    if(ready) { assert(ok==expected && history==is_history); cJSON_Delete(reply); }
    return ready;
}
int main(void)
{
    uint32_t owner=ha_alarm_panel_open(); assert(owner);
    assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),false)==ESP_OK);
    assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),false)==ESP_ERR_INVALID_STATE);
    ha_alarm_panel_cancel(owner); drain(); assert(sent==0 && !poll(owner,false,false));
    assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),false)==ESP_OK);
    test_now+=REPLY_TIMEOUT_US; drain(); assert(sent==0 && poll(owner,false,false));
    assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),false)==ESP_OK);
    drain(); assert(sent==1); callback(true,NULL,callback_user);
    callback(false,NULL,callback_user); assert(poll(owner,true,false));
    assert(!poll(owner,false,false));
    assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),false)==ESP_OK);
    drain(); void *old_user=callback_user;
    ha_alarm_panel_close(owner); owner=ha_alarm_panel_open(); assert(owner);
    assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),false)==ESP_OK);
    callback(true,NULL,old_user); assert(!poll(owner,false,false));
    drain(); test_now+=REPLY_TIMEOUT_US; callback(true,NULL,callback_user);
    assert(poll(owner,false,false)); /* A reply after the deadline cannot become success. */
    assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),true)==ESP_OK); drain();
    cJSON *rows=cJSON_CreateArray(); for(int i=0;i<60;++i) cJSON_AddItemToArray(rows,cJSON_CreateNumber(i));
    callback(true,rows,callback_user); cJSON_Delete(rows);
    bool ok,history; cJSON *reply=NULL;
    assert(ha_alarm_panel_poll(owner,&ok,&history,&reply) && ok && history);
    assert(cJSON_GetArraySize(reply)==40 && cJSON_GetArrayItem(reply,0)->valueint==20);
    cJSON_Delete(reply);
    assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),true)==ESP_OK); drain();
    callback(true,NULL,callback_user); assert(poll(owner,false,true));
    reject=true; assert(ha_alarm_panel_submit(owner,cJSON_CreateObject(),false)==ESP_OK); drain();
    assert(poll(owner,false,false));
    uint32_t owners[MAILBOXES-1];
    for(unsigned i=0;i<MAILBOXES-1;++i) { owners[i]=ha_alarm_panel_open(); assert(owners[i]); }
    assert(!ha_alarm_panel_open());
    for(unsigned i=0;i<16;++i) assert(ha_alarm_panel_submit(owners[i],cJSON_CreateObject(),false)==ESP_OK);
    assert(ha_alarm_panel_submit(owners[16],cJSON_CreateObject(),false)==ESP_ERR_NO_MEM);
    assert(poll(owners[16],false,false));
    for(unsigned i=0;i<MAILBOXES-1;++i) ha_alarm_panel_close(owners[i]);
    unsigned before=sent; drain(); assert(sent==before);
    ha_alarm_panel_close(owner); free(s_boxes);
    puts("alarm mailboxes: cancellation, stale replies, timeouts, capacity, queue-full and bounded history passed");
}
'''

class AlarmClientTest(unittest.TestCase):
    def test_mailbox_lifetime(self):
        compiler = shutil.which(os.environ.get("CC", "cc")) or shutil.which("gcc")
        if not compiler:
            self.skipTest("native C compiler required")
        cjson = ROOT / "managed_components/espressif__cjson/cJSON"
        if (cjson / "cJSON.c").is_file():
            cjson_flags = ["-I", str(cjson)]
            cjson_link = [str(cjson / "cJSON.c")]
        else:
            # Fresh CI checkouts use the same system dependency as tools/test_host.sh.
            cjson_flags = shlex.split(subprocess.check_output(
                ["pkg-config", "--cflags", "libcjson"], text=True))
            cjson_link = shlex.split(subprocess.check_output(
                ["pkg-config", "--libs", "libcjson"], text=True))
        with tempfile.TemporaryDirectory(prefix="betta_alarm_client_") as temp:
            folder = Path(temp)
            for name, text in STUBS.items():
                path = folder / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
            source = folder / "test.c"
            source.write_text(HARNESS)
            binary = folder / ("test.exe" if os.name == "nt" else "test")
            subprocess.run([compiler, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                            "-I", str(folder), "-I", str(ROOT / "main"), *cjson_flags,
                            str(source), *cjson_link, "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    unittest.main()
