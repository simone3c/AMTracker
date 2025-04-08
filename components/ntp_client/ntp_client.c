#include "ntp_client.h"

#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "lwip/ip_addr.h"
#include "nvs_flash.h"

int get_ntp_clock(){

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("0.it.pool.ntp.org");

    esp_netif_sntp_init(&config);

    if(esp_netif_sntp_sync_wait(1000) != ESP_OK)
        return 1;

    esp_netif_sntp_deinit();

    return 0;
}