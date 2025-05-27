#ifndef WEB_INTERFACE
#define WEB_INTERFACE

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"

#include "my_err.h"

int web_ui_start();
void web_ui_stop();
int web_ui_wait_for_credentials();

#endif