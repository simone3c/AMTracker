#include "my_wifi.h"
#include "esp_log.h"
#include "esp_event_base.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "wifi_credentials.h"

// event group to contain status information
static EventGroupHandle_t wifi_event_group;
static int s_retry_num = 0;

// event handler for wifi events
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data){
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
		// ESP_LOGI("Wifi", "Connecting to AP...");
		esp_wifi_connect();
        return;
	}

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
		if (s_retry_num < MAX_FAILURES){
			// ESP_LOGI("Wifi", "Reconnecting to AP...");
			esp_wifi_connect();
			++s_retry_num;
            return;
		}
        xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
	}
}

// event handler for ip events
static void ip_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data){
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){

        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("Wifi", "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
    }
}

// connect to wifi and return the result
esp_err_t connect_wifi(){
	int status = WIFI_FAILURE;

	//initialize the esp network interface
	ESP_ERROR_CHECK(esp_netif_init());

	//initialize default esp event loop
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	//create wifi station in the wifi driver
	esp_netif_create_default_wifi_sta();

	//setup wifi station with the default wifi configuration
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /** EVENT LOOP CRAZINESS **/
	wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t wifi_handler_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &wifi_handler_event_instance));

    esp_event_handler_instance_t got_ip_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &ip_event_handler,
        NULL,
        &got_ip_event_instance));

    /** START THE WIFI DRIVER **/
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PWD,
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    // set the wifi controller to be a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    // start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("Wifi", "STA initialization complete");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_SUCCESS | WIFI_FAILURE,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_SUCCESS) {
        ESP_LOGI("Wifi", "Connected to ap");
        status = WIFI_SUCCESS;
    } else if (bits & WIFI_FAILURE) {
        ESP_LOGI("Wifi", "Failed to connect to ap");
        status = WIFI_FAILURE;
    } else {
        ESP_LOGE("Wifi", "UNEXPECTED EVENT");
        status = WIFI_FAILURE;
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    vEventGroupDelete(wifi_event_group);

    return status;
}

void setup_wifi(){

    // initialize storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_err_t status = WIFI_FAILURE;

    // connect to wireless AP
	status = connect_wifi();
	if (WIFI_SUCCESS != status){
		ESP_LOGI("Wifi", "Failed to associate to AP...");
		return;
    }

}

