#ifndef WEB_INTERFACE
#define WEB_INTERFACE

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"

#include "my_err.h"

int start_web_interface();
void stop_web_interface();
int wait_for_credentials();

#endif