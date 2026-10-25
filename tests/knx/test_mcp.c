/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "knx/knx.h"
#include "knx/knx_mcp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned calls;
static bool large_page;
cJSON* knx_status(void) { return cJSON_Parse("{\"enabled\":false,\"state\":\"disabled\"}"); }
cJSON* knx_objects(int offset,int limit) {
    assert(offset==0 && limit==5);
    cJSON* result=cJSON_Parse("{\"objects\":[],\"has_more\":false,\"next_offset\":5}");
    if (large_page) {
        char description[193]; memset(description,'"',192); description[192]=0;
        cJSON* list=cJSON_GetObjectItemCaseSensitive(result,"objects");
        for (int i=0;i<limit;i++) {
            cJSON* o=cJSON_CreateObject(); cJSON_AddItemToArray(list,o);
            cJSON_AddStringToObject(o,"description",description);
            cJSON_AddStringToObject(o,"name",description);
        }
    }
    return result;
}
cJSON* knx_object(const char* id) {
    return !strcmp(id,"sensor") ? cJSON_Parse("{\"id\":\"sensor\",\"valid\":false}") : NULL;
}
bool knx_command(const char* id,const char* address,const char* value,bool write,char* err,size_t size) {
    calls++;
    assert((id && !address) || (!id && address));
    bool allowed = id && !strcmp(id,"lamp") && write && !strcmp(value,"on");
    if (!allowed) snprintf(err,size,"not permitted");
    return allowed;
}
static cJSON* request(const char* text) {
    cJSON* j=cJSON_Parse(text); assert(j); cJSON* r=knx_mcp_handle(j); cJSON_Delete(j); return r;
}
static cJSON* call(const char* name,const char* args) {
    char text[1024]; snprintf(text,sizeof(text),"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"%s\",\"arguments\":%s}}",name,args);
    return request(text);
}
int main(void) {
    /* Speech purge must retain MCP handshake/calls while ignoring old speech. */
    const char* messages[]={
        "{\"type\":\"tts\",\"state\":\"stop\"}",
        "{\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"initialize\"}}",
        "{\"type\":\"stt\",\"text\":\"old speech\"}",
        "{\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/list\"}}",
        "{\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}}"
    };
    int replies=0;
    for (size_t i=0;i<sizeof(messages)/sizeof(messages[0]);i++) {
        cJSON* m=cJSON_Parse(messages[i]); cJSON* response=knx_mcp_message(m,"test-session");
        if (response) {
            assert(i==1 || i==3); replies++;
            assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(response,"session_id")),"test-session"));
            cJSON* payload=cJSON_GetObjectItemCaseSensitive(response,"payload");
            assert(cJSON_GetObjectItemCaseSensitive(payload,"id")->valueint==(i==1?7:8));
            assert(cJSON_GetObjectItemCaseSensitive(payload,"result"));
        } else assert(i!=1 && i!=3);
        cJSON_Delete(response); cJSON_Delete(m);
    }
    assert(replies==2 && calls==0);
    assert(!request("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}"));
    cJSON* r=request("{\"jsonrpc\":\"2.0\",\"id\":\"init\",\"method\":\"initialize\"}");
    assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(r,"id")),"init")); cJSON_Delete(r);
    r=request("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}");
    char* text=cJSON_PrintUnformatted(r); assert(strlen(text)<6000);
    assert(strstr(text,"self.knx.set_object") && !strstr(text,"import_configuration"));
    free(text); cJSON_Delete(r);
    r=call("self.knx.list_objects","{}"); assert(cJSON_GetObjectItemCaseSensitive(r,"result")); cJSON_Delete(r);
    large_page=true; r=call("self.knx.list_objects","{}");
    cJSON* result=cJSON_GetObjectItemCaseSensitive(r,"result");
    cJSON* item=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(result,"content"),0);
    cJSON* page=cJSON_Parse(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(item,"text")));
    int count=cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(page,"objects"));
    assert(count>0 && count<5);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(page,"has_more")));
    assert(cJSON_GetObjectItemCaseSensitive(page,"next_offset")->valueint==count);
    text=cJSON_PrintUnformatted(r); assert(strlen(text)<8192); free(text);
    cJSON_Delete(page);cJSON_Delete(r);large_page=false;
    const char* bad[]={"{\"limit\":0}","{\"limit\":1.2}","{\"limit\":6}","{\"offset\":-1}","{\"limit\":1,\"limit\":2}","[]","{\"secret\":1}"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        r=call("self.knx.list_objects",bad[i]); assert(cJSON_GetObjectItemCaseSensitive(r,"error")); cJSON_Delete(r);
    }
    r=call("self.knx.set_object","{\"object_id\":\"lamp\",\"value\":\"on\"}");
    assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(r,"result"),"isError"))); cJSON_Delete(r);
    assert(calls==1);
    r=call("self.knx.set_object","{\"object_id\":\"sensor\",\"value\":\"on\"}");
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(r,"result"),"isError"))); cJSON_Delete(r);
    r=call("self.knx.set_object","{\"object_id\":\"lamp\",\"value\":true}");
    assert(cJSON_GetObjectItemCaseSensitive(r,"error")); cJSON_Delete(r); assert(calls==2);
    r=call("self.knx.import_configuration","{}"); assert(cJSON_GetObjectItemCaseSensitive(r,"error")); cJSON_Delete(r);
    r=call("self.knx.get_object","{\"object_id\":\"sensor\"}");
    text=cJSON_PrintUnformatted(r); assert(strstr(text,"valid")); free(text); cJSON_Delete(r);
    puts("KNX MCP discovery, argument validation, permission dispatch and errors passed");
}
