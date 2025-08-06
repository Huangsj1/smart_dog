#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#include "esp_http_client.h"

esp_err_t http_event_handler(esp_http_client_event_t *evt);
void request(const char* url);
#endif