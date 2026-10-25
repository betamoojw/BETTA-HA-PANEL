/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "knx_mcp.h"
#include "knx.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { const char* name; const char* description; const char* schema; } tool_t;
/* No configuration/enable tool is advertised to the model: the browser owns
 * the authorization boundary. Values remain strings to preserve 64-bit DPTs. */
static const tool_t tools[] = {
    {"self.knx.get_status", "KNX routing availability. Running means multicast is joined, not that the bus is confirmed reachable.",
     "{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}"},
    {"self.knx.list_objects", "Discover configured KNX IDs, names, descriptions, DPTs and permissions before selecting an object. Use next_offset while has_more is true.",
     "{\"type\":\"object\",\"properties\":{\"offset\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":128},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":5}},\"additionalProperties\":false}"},
    {"self.knx.get_object", "Get KNX metadata and cached state by exact ID. valid=false means unknown. Check state and last_update_ms; never invent confirmation.",
     "{\"type\":\"object\",\"properties\":{\"object_id\":{\"type\":\"string\"}},\"required\":[\"object_id\"],\"additionalProperties\":false}"},
    {"self.knx.set_object", "Control a configured writable KNX object by exact ID. value is a string: on/off for switches, decimal for numbers, comma-separated fields for compound DPTs. Queued is not bus confirmation.",
     "{\"type\":\"object\",\"properties\":{\"object_id\":{\"type\":\"string\"},\"value\":{\"type\":\"string\"}},\"required\":[\"object_id\",\"value\"],\"additionalProperties\":false}"},
    {"self.knx.read", "Request an asynchronous group read of a configured readable address. Later use get_object for feedback.",
     "{\"type\":\"object\",\"properties\":{\"group_address\":{\"type\":\"string\"}},\"required\":[\"group_address\"],\"additionalProperties\":false}"},
    {"self.knx.write", "Fallback write by configured writable group address. Prefer set_object with a discovered ID. DPT is fixed by configuration.",
     "{\"type\":\"object\",\"properties\":{\"group_address\":{\"type\":\"string\"},\"value\":{\"type\":\"string\"}},\"required\":[\"group_address\",\"value\"],\"additionalProperties\":false}"}
};
static const char* string(const cJSON* j, const char* key) {
    const char* s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j,key)); return s ? s : "";
}
cJSON* knx_mcp_message(const cJSON* message, const char* session) {
    if (strcmp(string(message, "type"), "mcp")) return NULL;
    cJSON* result = knx_mcp_handle(cJSON_GetObjectItemCaseSensitive(message, "payload"));
    if (!result) return NULL;
    cJSON* response = cJSON_CreateObject();
    if (!response) { cJSON_Delete(result); return NULL; }
    cJSON_AddStringToObject(response, "type", "mcp");
    cJSON_AddStringToObject(response, "session_id", session ? session : "");
    cJSON_AddItemToObject(response, "payload", result);
    return response;
}
static cJSON* rpc_error(cJSON* response, int code, const char* message) {
    cJSON* e = cJSON_AddObjectToObject(response, "error");
    cJSON_AddNumberToObject(e, "code", code); cJSON_AddStringToObject(e, "message", message); return response;
}
static bool arguments(const cJSON* args, const char* schema_text) {
    if (args && !cJSON_IsObject(args)) return false;
    cJSON* schema = cJSON_Parse(schema_text);
    const cJSON* props = cJSON_GetObjectItemCaseSensitive(schema, "properties");
    const cJSON* required = cJSON_GetObjectItemCaseSensitive(schema, "required");
    bool ok = schema != NULL;
    for (const cJSON* p = required ? required->child : NULL; p; p = p->next)
        if (!cJSON_GetObjectItemCaseSensitive(args, p->valuestring)) ok = false;
    for (const cJSON* p = args ? args->child : NULL; p; p = p->next) {
        const cJSON* rule = cJSON_GetObjectItemCaseSensitive(props, p->string);
        if (!rule) { ok = false; break; }
        for (const cJSON* q = p->next; q; q=q->next) if (!strcmp(p->string,q->string)) ok=false;
        if (!strcmp(string(rule,"type"), "integer")) {
            const cJSON* min = cJSON_GetObjectItemCaseSensitive(rule, "minimum");
            const cJSON* max = cJSON_GetObjectItemCaseSensitive(rule, "maximum");
            if (!cJSON_IsNumber(p) || !isfinite(p->valuedouble) || p->valuedouble != p->valueint ||
                p->valuedouble < min->valuedouble || p->valuedouble > max->valuedouble) ok=false;
        } else if (!cJSON_IsString(p) || !p->valuestring[0] || strlen(p->valuestring)>255) ok=false;
    }
    cJSON_Delete(schema); return ok;
}
cJSON* knx_mcp_handle(const cJSON* request) {
    const cJSON* id = cJSON_GetObjectItemCaseSensitive(request, "id");
    if (!id) return NULL;
    cJSON* response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, false));
    if (!cJSON_IsObject(request) || strcmp(string(request,"jsonrpc"),"2.0") ||
        (!cJSON_IsNumber(id) && !cJSON_IsString(id))) return rpc_error(response,-32600,"Invalid request");
    const char* method = string(request, "method");
    const cJSON* params = cJSON_GetObjectItemCaseSensitive(request, "params");
    if (!strcmp(method, "initialize")) {
        cJSON* result = cJSON_AddObjectToObject(response, "result");
        cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
        cJSON_AddItemToObject(result, "capabilities", cJSON_Parse("{\"tools\":{}}"));
        cJSON_AddItemToObject(result, "serverInfo", cJSON_Parse("{\"name\":\"BETTA KNX\",\"version\":\"1.0\"}"));
    } else if (!strcmp(method, "ping")) cJSON_AddObjectToObject(response, "result");
    else if (!strcmp(method, "tools/list")) {
        if (cJSON_GetObjectItemCaseSensitive(params, "cursor")) return rpc_error(response,-32602,"Invalid cursor");
        cJSON* result = cJSON_AddObjectToObject(response, "result");
        cJSON* list = cJSON_AddArrayToObject(result, "tools");
        for (size_t i=0;i<sizeof(tools)/sizeof(tools[0]);i++) {
            cJSON* t = cJSON_CreateObject(); cJSON_AddItemToArray(list, t);
            cJSON_AddStringToObject(t,"name",tools[i].name);
            cJSON_AddStringToObject(t,"description",tools[i].description);
            cJSON_AddItemToObject(t,"inputSchema",cJSON_Parse(tools[i].schema));
        }
    } else if (!strcmp(method, "tools/call")) {
        const char* name = string(params,"name"); size_t index=0;
        while (index<sizeof(tools)/sizeof(tools[0]) && strcmp(name, tools[index].name)) index++;
        if (index==sizeof(tools)/sizeof(tools[0])) return rpc_error(response,-32602,"Unknown tool");
        const cJSON* args = cJSON_GetObjectItemCaseSensitive(params,"arguments");
        if (!arguments(args,tools[index].schema)) return rpc_error(response,-32602,"Invalid tool arguments");
        cJSON* data=NULL; char error[160]="Unknown KNX object";
        if (index==0) data=knx_status();
        else if (index==1) {
            const cJSON* off=cJSON_GetObjectItemCaseSensitive(args,"offset");
            const cJSON* lim=cJSON_GetObjectItemCaseSensitive(args,"limit");
            data=knx_objects(off?off->valueint:0,lim?lim->valueint:5);
        } else if (index==2) data=knx_object(string(args,"object_id"));
        else if (knx_command(index==3?string(args,"object_id"):NULL,
                 index==3?NULL:string(args,"group_address"), string(args,"value"), index!=4, error,sizeof(error)))
            data=cJSON_Parse("{\"queued\":true,\"completion\":\"asynchronous\",\"bus_confirmed\":false}");
        // Compound values/escaped site names can exceed an audio frame even
        // with five objects. Shrink this page and keep its continuation cursor.
        if (index==1 && data) {
            cJSON* list=cJSON_GetObjectItemCaseSensitive(data,"objects");
            while (cJSON_GetArraySize(list)>1) {
                char* rendered=cJSON_PrintUnformatted(data);
                bool fits=rendered && strlen(rendered)<=3200; free(rendered);
                if (fits) break;
                cJSON_DeleteItemFromArray(list,cJSON_GetArraySize(list)-1);
                cJSON_ReplaceItemInObjectCaseSensitive(data,"has_more",cJSON_CreateBool(true));
                cJSON* off=cJSON_GetObjectItemCaseSensitive(args,"offset");
                cJSON_ReplaceItemInObjectCaseSensitive(data,"next_offset",cJSON_CreateNumber(
                    (off?off->valueint:0)+cJSON_GetArraySize(list)));
            }
        }
        cJSON* result=cJSON_AddObjectToObject(response,"result");
        cJSON_AddBoolToObject(result,"isError",data==NULL);
        cJSON* content=cJSON_AddArrayToObject(result,"content"), *item=cJSON_CreateObject();
        cJSON_AddItemToArray(content,item); cJSON_AddStringToObject(item,"type","text");
        char* text=data?cJSON_PrintUnformatted(data):NULL;
        cJSON_AddStringToObject(item,"text",text?text:error);
        free(text); cJSON_Delete(data);
    } else return rpc_error(response,-32601,"Method not found");
    return response;
}
