#ifndef MY_WIFI
#define MY_WIFI

#include "esp_wifi.h"

#define WIFI_SUCCESS 1
#define WIFI_FAILURE 2
#define WIFI_STA_RUNNING 3
#define MAX_FAILURES 10

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct addr_in addr_in;

esp_err_t wifi_sta_setup(wifi_config_t* sta_cfg);
esp_err_t wifi_ap_setup(wifi_config_t* ap_cfg);
// AP starts as soon as possible(that is, after calling wifi_start()). STA tries to connect (after calling wifi_start())
// only if sta_cfg.ap.ssid is not empty,
// so you can postpone the STA connection by passing sta_cfg.ap.ssid as an empty field.
// you have to call wifi_apsta_connect_to() with a new sta_cfg with a valid ssid to start
// the STA connection process at an arbitraty time.
esp_err_t wifi_apsta_setup(wifi_config_t* sta_cfg, wifi_config_t* ap_cfg);
esp_err_t wifi_start();
void wifi_stop();
void wifi_stop_and_deinit();
esp_err_t wifi_apsta_connect_to(wifi_config_t* sta_cfg);
esp_err_t wifi_scan(wifi_ap_record_t* ap_info, uint16_t* ap_count);

#endif