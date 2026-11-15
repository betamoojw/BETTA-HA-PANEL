/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#define main run_account_tests
#define path_is policy_path_is
#include "test_user_auth.c"
#undef main
#undef path_is
#include "api/http_guard.c"
#include "api/http_user_auth.c"
#include <stdarg.h>

void host_log(const char *level, const char *tag, const char *fmt, ...) { (void)level; (void)tag; (void)fmt; }
void user_activity_note(uint32_t seconds) { (void)seconds; }
int getpeername(int fd, struct sockaddr *peer, socklen_t *length) {
    (void)fd; struct sockaddr_in *p = (void *)peer; p->sin_family = AF_INET; p->sin_addr.s_addr = 42; *length = sizeof(*p); return 0;
}
int httpd_req_to_sockfd(httpd_req_t *req) { (void)req; return 1; }
static const char *header(httpd_req_t *r, const char *name) {
    if (!strcmp(name,"Cookie")) return r->cookie;
    if (!strcmp(name,"Authorization")) return r->authorization;
    if (!strcmp(name,"Origin")) return r->origin;
    if (!strcmp(name,"Host")) return "panel.local";
    if (!strcmp(name,"X-Betta-Request")) return r->marker;
    if (!strcmp(name,"Content-Type")) return r->content_type;
    return NULL;
}
size_t httpd_req_get_hdr_value_len(httpd_req_t *r, const char *name) { const char *v = header(r,name); return v ? strlen(v) : 0; }
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r, const char *name, char *out, size_t n) {
    const char *v = header(r,name); if (!v || strlen(v) >= n) return ESP_FAIL; strcpy(out,v); return ESP_OK;
}
esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *status) { snprintf(r->status,sizeof(r->status),"%s",status); return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type) { (void)r; (void)type; return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *name, const char *value) {
    if (!strcmp(name,"Set-Cookie")) snprintf(r->set_cookie,sizeof(r->set_cookie),"%s",value);
    return ESP_OK;
}
esp_err_t httpd_resp_sendstr(httpd_req_t *r, const char *text) { snprintf(r->response,sizeof(r->response),"%s",text); return ESP_OK; }
esp_err_t httpd_resp_send_500(httpd_req_t *r) { return reply(r,"500 Internal Server Error","{}"); }
esp_err_t httpd_resp_send_404(httpd_req_t *r) { return reply(r,"404 Not Found","{}"); }
int httpd_req_recv(httpd_req_t *r, char *out, size_t n) {
    size_t left = r->content_len - r->consumed; if (n > left) n = left; if (n > 17) n = 17;
    memcpy(out,r->body+r->consumed,n); r->consumed += n; return n;
}
static int route_count;
esp_err_t httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t *uri) {
    (void)server; CHECK(uri->handler != NULL); ++route_count; return ESP_OK;
}
static unsigned called;
static esp_err_t protected_handler(httpd_req_t *r) {
    ++called;
    if (!strcmp(r->uri, "/api/settings")) {
        cJSON *settings = cJSON_Parse("{\"ok\":true,\"ui\":{\"language\":\"de\"},\"hardware\":{},\"topbar\":{},\"wifi\":{\"ssid\":\"private\"},\"ha\":{\"ws_url\":\"private\"},\"mqtt\":{},\"network\":{},\"new_secret_field\":\"private\"}");
        http_user_auth_filter_settings(settings);
        return send_object(r, settings);
    }
    return httpd_resp_sendstr(r,"{\"ok\":true}");
}
static httpd_req_t request(int method, const char *uri, const char *body, const char *cookie, const char *origin, const char *marker) {
    clock_us += 1000000;
    httpd_req_t r = {.uri=uri,.method=method,.body=body,.content_len=body?(int)strlen(body):0,
        .cookie=cookie,.origin=origin,.marker=marker,.content_type="application/json"};
    strcpy(r.status,"200 OK");
    if (strncmp(uri,"/api/auth/",10)==0 || !strcmp(uri,"/api/users")) auth_handler(&r);
    else http_guard_handle(&r,protected_handler);
    return r;
}
static void expect(httpd_req_t r, int status) { CHECK(atoi(r.status) == status); }
static void login(const char *name, char *cookie) {
    char body[160]; snprintf(body,sizeof(body),"{\"username\":\"%s\",\"password\":\"password1\"}",name);
    httpd_req_t r = request(HTTP_POST,"/api/auth/login",body,NULL,"http://panel.local","1"); expect(r,200);
    CHECK(strstr(r.set_cookie,"HttpOnly")); CHECK(strstr(r.set_cookie,"SameSite=Strict"));
    CHECK(!strstr(r.response,"password1")); CHECK(!strstr(r.response,"token"));
    snprintf(cookie,96,"%.*s",78,r.set_cookie);
}
#ifndef LICENSE_HTTP_REUSE
int main(void) {
    fresh(); http_user_auth_register((void *)1); CHECK(route_count == 6);
    expect(request(HTTP_GET,"/api/auth/session",NULL,NULL,NULL,NULL),200);
    expect(request(HTTP_POST,"/api/users","{\"action\":\"create\",\"username\":\"owner\",\"password\":\"password1\",\"role\":\"administrator\",\"enabled\":true}",NULL,"http://evil.example","1"),403);
    CHECK(user_auth_state() == USER_AUTH_OPEN);
    CHECK(user_auth_update("owner","password1",USER_ROLE_ADMIN,true,true)==ESP_OK);
    CHECK(user_auth_update("reader","password1",USER_ROLE_VIEWER,true,true)==ESP_OK);
    CHECK(user_auth_update("operator","password1",USER_ROLE_OPERATOR,true,true)==ESP_OK);
    CHECK(user_auth_update("installer","password1",USER_ROLE_INSTALLER,true,true)==ESP_OK);
    expect(request(HTTP_GET,"/api/layout",NULL,NULL,NULL,NULL),401);
    CHECK(called==0);
    char owner[96],reader[96],op[96],installer[96];
    login("owner",owner); login("reader",reader); login("operator",op); login("installer",installer);
    expect(request(HTTP_GET,"/api/layout",NULL,reader,NULL,NULL),200);
    httpd_req_t settings = request(HTTP_GET,"/api/settings",NULL,reader,NULL,NULL);
    expect(settings,200); CHECK(strstr(settings.response,"language")); CHECK(!strstr(settings.response,"private"));
    CHECK(!strstr(settings.response,"network")); CHECK(!strstr(settings.response,"new_secret_field"));
    settings = request(HTTP_GET,"/api/settings",NULL,installer,NULL,NULL);
    expect(settings,200); CHECK(strstr(settings.response,"hardware")); CHECK(!strstr(settings.response,"private"));
    settings = request(HTTP_GET,"/api/settings",NULL,owner,NULL,NULL);
    expect(settings,200); CHECK(strstr(settings.response,"private"));
    char duplicate[194]; snprintf(duplicate,sizeof(duplicate),"%s; %s",reader,reader);
    expect(request(HTTP_GET,"/api/layout",NULL,duplicate,NULL,NULL),401);
    expect(request(HTTP_PUT,"/api/layout","{}",reader,"http://panel.local","1"),403);
    expect(request(HTTP_POST,"/api/navigate","{}",reader,"http://panel.local","1"),403);
    expect(request(HTTP_POST,"/api/navigate","{}",op,"http://panel.local","1"),200);
    expect(request(HTTP_PUT,"/api/layout","{}",installer,"http://panel.local","1"),200);
    expect(request(HTTP_PUT,"/api/settings","{}",installer,"http://panel.local","1"),403);
    expect(request(HTTP_GET,"/api/users",NULL,reader,NULL,NULL),403);
    expect(request(HTTP_GET,"/api/coredump?id=123",NULL,op,NULL,NULL),403);
    expect(request(HTTP_POST,"/api/ota/url","{}",installer,"http://panel.local","1"),403);
    expect(request(HTTP_POST,"/api/factory-reset","{}",op,"http://panel.local","1"),403);
    expect(request(HTTP_POST,"/api/coredump/erase","{}",owner,"http://evil.example","1"),403);
    expect(request(HTTP_POST,"/api/coredump/erase","{}",owner,NULL,NULL),403);
    expect(request(HTTP_DELETE,"/api/themes/custom",NULL,owner,"http://evil.example","1"),403);
    httpd_req_t list=request(HTTP_GET,"/api/users",NULL,owner,NULL,NULL); expect(list,200);
    CHECK(strstr(list.response,"reader")); CHECK(!strstr(list.response,"hash")); CHECK(!strstr(list.response,"password"));
    expect(request(HTTP_POST,"/api/users","{\"action\":\"delete\",\"username\":\"owner\"}",owner,"http://panel.local","1"),400);
    expect(request(HTTP_POST,"/api/users","{\"action\":\"delete\",\"username\":\"reader\",\"username\":\"owner\"}",owner,"http://panel.local","1"),400);
    expect(request(HTTP_POST,"/api/auth/password","{\"current_password\":\"wrong\",\"new_password\":\"password2\"}",reader,"http://panel.local","1"),401);
    expect(request(HTTP_POST,"/api/auth/password","{\"current_password\":\"password1\",\"new_password\":\"password2\"}",reader,"http://panel.local","1"),200);
    expect(request(HTTP_GET,"/api/users",NULL,owner,NULL,NULL),401);
    login("owner",owner);
    expect(request(HTTP_POST,"/api/users","{\"action\":\"update\",\"username\":\"reader\",\"password\":\"\",\"role\":\"operator\",\"enabled\":false}",owner,"http://panel.local","1"),200);
    login("owner",owner);
    expect(request(HTTP_POST,"/api/auth/logout","{}",owner,"http://panel.local","1"),200);
    expect(request(HTTP_GET,"/api/users",NULL,owner,NULL,NULL),401);
    for (int i=0;i<6;++i) expect(request(HTTP_POST,"/api/auth/login","{\"username\":\"reader\",\"password\":\"password1\"}",NULL,"http://panel.local","1"),401);
    expect(request(HTTP_POST,"/api/auth/login","{\"username\":\"owner\",\"password\":\"password1\"}",NULL,"http://panel.local","1"),429);
    clock_us += 61000000; login("owner",owner);
    httpd_req_t basic={.uri="/api/users",.method=HTTP_GET,.authorization="Basic b3duZXI6cGFzc3dvcmQx"};
    auth_handler(&basic); expect(basic,200); CHECK(strstr(basic.response,"owner"));
    basic=(httpd_req_t){.uri="/api/settings",.method=HTTP_PUT,.authorization="Basic b3duZXI6cGFzc3dvcmQx",.content_type="text/plain"};
    http_guard_handle(&basic,protected_handler); expect(basic,415);
    printf("HTTP user lifecycle: %u checks passed\n",checks); return 0;
}
#endif
