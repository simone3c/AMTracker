#ifndef WEB_INTERFACE
#define WEB_INTERFACE

#include "esp_http_server.h"

#include "my_err.h"

int web_ui_start();
void web_ui_stop();
int web_ui_wait_for_credentials();
void web_ui_get_credentials(uint8_t* ssid, uint8_t* pwd);

#endif