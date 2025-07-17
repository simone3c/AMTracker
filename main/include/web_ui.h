#ifndef WEB_INTERFACE
#define WEB_INTERFACE

#include "esp_err.h"

int web_ui_start();
void web_ui_stop();
int web_ui_wait_for_credentials();
esp_err_t web_ui_get_credentials(uint8_t* ssid, size_t* ssid_len, uint8_t* pwd, size_t* pwd_len);
esp_err_t web_ui_notify_connection_established();
esp_err_t web_ui_notify_connection_failed();

#endif