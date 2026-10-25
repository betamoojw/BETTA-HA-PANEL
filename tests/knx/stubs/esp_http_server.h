#pragma once
#include "esp_err.h"
#include <stddef.h>
typedef void* httpd_handle_t;
typedef struct { size_t content_len; } httpd_req_t;
typedef struct { const char* uri; int method; esp_err_t (*handler)(httpd_req_t*); } httpd_uri_t;
enum { HTTP_GET, HTTP_PUT, HTTP_POST };
enum { HTTPD_400_BAD_REQUEST=400, HTTPD_408_REQ_TIMEOUT=408 };
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t httpd_resp_send_500(httpd_req_t*);
esp_err_t httpd_resp_set_type(httpd_req_t*,const char*);
esp_err_t httpd_resp_set_hdr(httpd_req_t*,const char*,const char*);
esp_err_t httpd_resp_set_status(httpd_req_t*,const char*);
esp_err_t httpd_resp_sendstr(httpd_req_t*,const char*);
esp_err_t httpd_resp_send_err(httpd_req_t*,int,const char*);
int httpd_req_recv(httpd_req_t*,char*,size_t);
esp_err_t httpd_req_get_url_query_str(httpd_req_t*,char*,size_t);
esp_err_t httpd_query_key_value(const char*,const char*,char*,size_t);
esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*);
#ifdef __cplusplus
}
#endif
