/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723 */
#include "knx/knx.h"
#include "api/http_guard.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static httpd_uri_t routes[6];
static unsigned route_count,guard_calls,commands;
static int status;
static const char* incoming;
static size_t consumed;
static char response[1024];
esp_err_t httpd_register_uri_handler(httpd_handle_t s,const httpd_uri_t* r) {
    (void)s; assert(route_count<6);routes[route_count++]=*r;return ESP_OK;
}
esp_err_t http_guard_handle(httpd_req_t* r,http_guard_handler_t next) {guard_calls++;return next(r);}
esp_err_t httpd_resp_set_type(httpd_req_t* r,const char* v) {(void)r;(void)v;return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t* r,const char* k,const char* v) {(void)r;(void)k;(void)v;return ESP_OK;}
esp_err_t httpd_resp_set_status(httpd_req_t* r,const char* v) {(void)r;status=atoi(v);return ESP_OK;}
esp_err_t httpd_resp_sendstr(httpd_req_t* r,const char* v) {(void)r;snprintf(response,sizeof(response),"%s",v);return ESP_OK;}
esp_err_t httpd_resp_send_err(httpd_req_t* r,int code,const char* v) {status=code;return httpd_resp_sendstr(r,v);}
esp_err_t httpd_resp_send_500(httpd_req_t* r) {return httpd_resp_send_err(r,500,"OOM");}
int httpd_req_recv(httpd_req_t* r,char* out,size_t n) {
    if (n>7) n=7;
    if (n>r->content_len-consumed) n=r->content_len-consumed;
    memcpy(out,incoming+consumed,n);consumed+=n;return (int)n;
}
esp_err_t httpd_req_get_url_query_str(httpd_req_t* r,char* q,size_t n) {(void)r;(void)q;(void)n;return ESP_FAIL;}
esp_err_t httpd_query_key_value(const char* q,const char* k,char* v,size_t n) {(void)q;(void)k;(void)v;(void)n;return ESP_FAIL;}
cJSON* knx_status(void) {return cJSON_Parse("{\"enabled\":false}");}
char* knx_configuration(void) {return strdup("{}");}
bool knx_enable(bool on,char* e,size_t n) {(void)on;(void)e;(void)n;return true;}
bool knx_configure(const char* j,char* e,size_t n) {(void)j;(void)e;(void)n;return true;}
cJSON* knx_objects(int offset,int limit) {(void)offset;(void)limit;return cJSON_CreateObject();}
bool knx_command(const char* id,const char* address,const char* value,bool write,char* e,size_t n) {
    (void)e;(void)n;assert(!address);assert(!strcmp(id,"客厅灯"));
    assert(!write || !strcmp(value,"on"));commands++;return true;
}
static void post(const char* text) {
    incoming=text;consumed=0;status=200;response[0]=0;
    httpd_req_t req={.content_len=strlen(text)};
    assert(routes[5].handler(&req)==ESP_OK);
}
int main(void) {
    assert(knx_api_register(NULL)==ESP_OK && route_count==6);
    post("{\"object_id\":\"客厅灯\",\"action\":\"write\",\"value\":\"on\"}");
    assert(status==202 && commands==1 && strstr(response,"\"bus_confirmed\":false"));
    post("{\"object_id\":\"客厅灯\",\"action\":\"read\"}");assert(status==202 && commands==2);
    const char* bad[]={"[]","{}","{\"object_id\":\"客厅灯\",\"action\":\"write\",\"value\":true}",
        "{\"object_id\":\"客厅灯\",\"action\":\"read\",\"extra\":1}",
        "{\"object_id\":\"客厅灯\",\"object_id\":\"客厅灯\",\"action\":\"read\"}",
        "{\"object_id\":\"客厅灯\\u0000wrong\",\"action\":\"read\"}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {post(bad[i]);assert(status==400);}
    char nested[2002];memset(nested,'[',1000);memset(nested+1000,']',1000);nested[2000]=0;
    post(nested);assert(status==400 && commands==2 && guard_calls==9);
    puts("Guarded KNX routes, chunked bodies, strict commands and recursion bounds passed");
}
