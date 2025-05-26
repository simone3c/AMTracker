#include "include/my_wifi.h"
#include "esp_log.h"
#include "esp_event_base.h"
#include "esp_system.h"
#include "esp_event.h"

static wifi_mode_t internal_wifi_mode = WIFI_MODE_NULL;
static esp_event_handler_instance_t wifi_handler_event_instance, got_ip_event_instance;

static esp_netif_t* sta_netif, *ap_netif;

#define WIFI_CONNECTED_BIT BIT0

// event group to contain status information
static EventGroupHandle_t wifi_event_group;
static int s_retry_num = 0;

void wifi_stop(){
    
    if(internal_wifi_mode == WIFI_MODE_NULL)
        return;

    switch(internal_wifi_mode){
    case WIFI_MODE_STA:
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = NULL;
    break;
    case WIFI_MODE_AP:
        esp_netif_destroy_default_wifi(ap_netif);
        ap_netif = NULL;
    break;
    case WIFI_MODE_APSTA:
        esp_netif_destroy_default_wifi(sta_netif);
        esp_netif_destroy_default_wifi(ap_netif);
        sta_netif = NULL;
        ap_netif = NULL;
    break;
    default:
        assert(false);
    break;
    }


    internal_wifi_mode = WIFI_MODE_NULL;
 
    esp_wifi_disconnect();
    
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    //esp_event_loop_delete_default(); // wifi should not call this function because other modules(bluetooth/...) may be using it
    
    vEventGroupDelete(wifi_event_group);


    ESP_ERROR_CHECK(esp_wifi_stop());
}

void wifi_stop_and_deinit(){
    wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

// event handler for wifi and IP events
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
		ESP_LOGI("wifi_event_handler", "Registered event: WIFI_EVENT_STA_START");
        
        if(internal_wifi_mode == WIFI_MODE_STA)
		    esp_wifi_connect();
	}

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
		if (s_retry_num < MAX_FAILURES){
		    ESP_LOGI("wifi_event_handler", "Registered event: WIFI_EVENT_STA_DISCONNECTED");
			esp_wifi_connect();
			++s_retry_num;
            return;
		}
        xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
	}

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI("wifi_event_handler", "Registered event: WIFI_EVENT_AP_STACONNECTED");
    } 
    
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI("wifi_event_handler", "Registered event: WIFI_EVENT_AP_STADISCONNECTED");
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("Wifi", "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// init STA wifi
void wifi_sta_init(wifi_config_t* sta_cfg){

    if(sta_cfg == NULL){
        return;
    }

	sta_netif = esp_netif_create_default_wifi_sta();

    // set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, sta_cfg));

    ESP_LOGI("wifi_sta_init", "STA initialization complete");
}

// init AP wifi
void wifi_ap_init(wifi_config_t* ap_cfg){

    if(ap_cfg == NULL)
        return;

    ap_netif = esp_netif_create_default_wifi_ap();

    if(strlen((char*)ap_cfg->ap.password) == 0) {
        ap_cfg->ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, ap_cfg));

    // ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
    //          EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_CHANNEL);

    ESP_LOGI("wifi_ap_init", "Finished to set up AP mode");
}

void wifi_apsta_init(wifi_config_t* sta_cfg, wifi_config_t* ap_cfg){
    wifi_sta_init(sta_cfg);
    wifi_ap_init(ap_cfg);
}

void wifi_setup(wifi_mode_t mode, wifi_config_t* sta_cfg, wifi_config_t* ap_cfg){

    // initialize storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //initialize the esp network interface
	ESP_ERROR_CHECK(esp_netif_init());
	//initialize default esp event loop
	ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_loop_create_default());

    //setup wifi station with the default wifi configuration
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    internal_wifi_mode = mode;
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    switch(mode){
    case WIFI_MODE_AP:
        wifi_ap_init(ap_cfg);
    break;
    case WIFI_MODE_STA:
        wifi_sta_init(sta_cfg);
    break;
    case WIFI_MODE_APSTA:
        wifi_apsta_init(sta_cfg, ap_cfg);
    break;
    default:
        assert(false);
    }

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &wifi_handler_event_instance)
    );

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        &got_ip_event_instance)
    );

}

void wifi_sta_setup(wifi_config_t* sta_cfg){
    wifi_setup(WIFI_MODE_STA, sta_cfg, NULL);
}

void wifi_apsta_setup(wifi_config_t* sta_cfg, wifi_config_t* ap_cfg){
    wifi_setup(WIFI_MODE_APSTA, sta_cfg, ap_cfg);
}

void wifi_ap_setup(wifi_config_t* ap_cfg){
    wifi_setup(WIFI_MODE_AP, NULL, ap_cfg);
}

int wait_sta_connection(){

    int status = -1;

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILURE,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI("wait_sta_connection", "Connected to ap");
        status = WIFI_CONNECTED_BIT;
    } else if (bits & WIFI_FAILURE) {
        ESP_LOGI("wait_sta_connection", "Failed to connect to ap");
        status = WIFI_FAILURE;
    } else {
        ESP_LOGE("wait_sta_connection", "UNEXPECTED EVENT");
        status = WIFI_FAILURE;
    }

    return status;
}

int wifi_start(){

    int status = -1;

    wifi_config_t sta_cfg;

    if(internal_wifi_mode == WIFI_MODE_NULL){
        ESP_LOGI("wifi_start", "cannot start WIFI without a previous configuration!!");
        return status;
    }

    // start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("Wifi_start", "STA initialization complete");

    switch(internal_wifi_mode){
    case WIFI_MODE_STA:
        status = wait_sta_connection();
    break;
    case WIFI_MODE_APSTA:
        ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &sta_cfg));
        if(strlen((char*)sta_cfg.sta.ssid) > 0){
            status = wait_sta_connection();
        }
        __attribute__ ((fallthrough));
    case WIFI_MODE_AP:
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    
        char ip_addr[16];
        inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
        ESP_LOGI("wifi_start", "Set up softAP with IP: %s", ip_addr);
    break;
    default:
        assert(false);
    break;
    }

    return status;
}

int wifi_apsta_connect_to(wifi_config_t* sta_cfg){

    if(internal_wifi_mode != WIFI_MODE_APSTA){
        ESP_LOGE("wifi_apsta_connect_to", "wifi mode is not set to APSTA!");
        return -1;
    }

    if(strlen((char*)sta_cfg->sta.ssid) == 0){
        ESP_LOGI("wifi_apsta_connect_to", "called wifi_apsta_connect_to() but ssid is not properly configured... strlen(ssid) == 0");
        return -2;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, sta_cfg));

    ESP_ERROR_CHECK(esp_wifi_connect());

    return wait_sta_connection();
}

int wifi_scan(wifi_ap_record_t* ap_info, uint16_t* ap_count){

    ESP_LOGE("wifi_scan", "here");

    if(esp_wifi_scan_start(NULL, true) != ESP_OK)
        return 1;
    if(esp_wifi_scan_get_ap_num(ap_count) != ESP_OK)
        return 2;
    if(esp_wifi_scan_get_ap_records(ap_count, ap_info) != ESP_OK)
        return 3;
    return 0;
}