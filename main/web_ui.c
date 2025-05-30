#include "my_wifi.h"
#include "web_ui.h"
#include "my_string.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs.h"


#define QUERY_LEN_MAX 100

#define INDEX_QUERY_KEY "method"
#define INDEX_QUERY_VALUE_LEN_MAX 4

#define CREDENTIALS_READY BIT0

// index.html
extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
// new_sta.html
extern const char new_sta_start[] asm("_binary_new_sta_html_start");
extern const char new_sta_end[] asm("_binary_new_sta_html_end");
// exit.html
extern const char exit_html_start[] asm("_binary_bye_html_start");
extern const char exit_html_end[] asm("_binary_bye_html_end");
// error.html
extern const char error_html_start[] asm("_binary_error_html_start");
extern const char error_html_end[] asm("_binary_error_html_end");

static EventGroupHandle_t internal_events = NULL;
static httpd_handle_t server = NULL;

// ! remember to free 'out' when not needed anymore
static my_string create_error_div(const char* err_msg, size_t err_msg_len){
    const char* error_div = "<div style=\"background-color: #f8d7da; color: #721c24; text-align: center;\"></div>";

    my_string ret;
    my_string_init_from_array(&ret, error_div, strlen(error_div));

    int at = my_string_strstr(&ret, "</div>");

    assert(at >= 0);
    my_string_insert_array_at(&ret, at, err_msg, err_msg_len);
    return ret;
}

// TODO
static esp_err_t new_sta_handler(httpd_req_t *req){
    static wifi_ap_record_t ap_info[20];
    uint16_t ap_count = 0;

    wifi_scan(ap_info, &ap_count);

    char content[100];
    memset(content, 0, 100);
    httpd_req_get_url_query_str(req, content, 100);
    ESP_LOGI("root_get_handler", "URL: '%s'", content);

    char tmp[32] = {0};
    char query[32] = {0};
    
    httpd_req_get_url_query_str(req, query, 32);
    httpd_query_key_value(query, "ssid", &tmp[0], 32);

    if(!strcmp(tmp, "v")){

        ESP_LOGI("root_get_handler", "Closing HTPP server");


        const uint32_t exit_len = exit_html_end - exit_html_start;

        ESP_LOGI("root_get_handler", "Serve root");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, exit_html_start, exit_len);

        xEventGroupSetBits(internal_events, CREDENTIALS_READY);

        return ESP_OK;
    }

    const uint32_t root_len = index_end - index_start;

    ESP_LOGI("root_get_handler", "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_start, root_len);

    return ESP_OK;
}

static esp_err_t send_new_sta_page(httpd_req_t* req){
    ESP_RETURN_ON_ERROR(
        httpd_resp_set_type(req, "text/html"), 
        "send_new_sta_page", 
        "cannot set response type"
    );
    ESP_RETURN_ON_ERROR(
        httpd_resp_send(req, new_sta_start, new_sta_end - new_sta_start), 
        "send_new_sta_page",
        "cannot send http response"
    );

    return ESP_OK;
}

// return the lengh of the string which contains the new div
static size_t add_error_div(my_string* page, const char* error_msg, size_t error_len){
    my_string error_div = create_error_div(error_msg, error_len);
    
    int at = my_string_strstr(page, "<body>") + strlen("<body>"); // points to the end of <body> inside index
    my_string_insert_at(page, at, &error_div);

    my_string_delete(&error_div);

    return page->len;
}

static esp_err_t send_html_page(httpd_req_t* req, const char* html, size_t len){
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, len);
}


static int index_handler(httpd_req_t *req){
    esp_err_t err = ESP_OK;

    uint8_t ssid[MAX_SSID_LEN] = {0};
    uint8_t pwd[MAX_PASSPHRASE_LEN] = {0};
    size_t ssid_len = MAX_SSID_LEN;
    size_t pwd_len = MAX_PASSPHRASE_LEN;

    my_string resp;
    my_string_init_from_array(&resp, index_start, index_end - index_start);

    esp_err_t credentials_status = web_ui_get_credentials(&ssid[0], &ssid_len, &pwd[0], &pwd_len);
    if(credentials_status != ESP_OK){
        ESP_LOGE("index_handler", "no NVS available");
        
        // disable button for using saved WiFi    
        int at = my_string_strstr(&resp, "></button>");
        assert(at >= 0);
        my_string_insert_array_at(&resp, at, "disabled", 8);   
    }

    int at = my_string_strstr(&resp, "</button>");
    assert(at >= 0);
    my_string_insert_array_at(&resp, at, (const char*)ssid, ssid_len);

    char query[QUERY_LEN_MAX] = {0};
    char value[INDEX_QUERY_VALUE_LEN_MAX] = {0};

    if(httpd_req_get_url_query_str(req, &query[0], QUERY_LEN_MAX) != ESP_OK){

        err = send_html_page(req, resp.data, resp.len);
        my_string_delete(&resp);
        
        return err;
    }

    ESP_LOGI("root_get_handler", "QUERY: '%s'", query);

    // bad requets (GET values) - responding with index.html + an error message in the page
    if(httpd_query_key_value(query, INDEX_QUERY_KEY, &value[0], INDEX_QUERY_VALUE_LEN_MAX) != ESP_OK || 
        (strcmp(value, "NVS") && strcmp(value, "NEW"))
    ){
        const char* error_msg = "Bad request";

        add_error_div(&resp, error_msg, strlen(error_msg));
        err = send_html_page(req, resp.data, resp.len);
        my_string_delete(&resp);

        return err;
    }

    if(strncmp(value, "NVS", 4) == 0){
        
        if(credentials_status != ESP_OK){
            const char* error_msg = "Unavailable credentials, try using a new WiFi network";

            add_error_div(&resp, error_msg, strlen(error_msg));
            err = send_html_page(req, resp.data, resp.len);
            my_string_delete(&resp);

            return err;
        }

        err = send_html_page(req, exit_html_start, exit_html_end - exit_html_start);
        
        xEventGroupSetBits(internal_events, CREDENTIALS_READY);
        my_string_delete(&resp);
        return err;
    }

    my_string_delete(&resp);
    return send_new_sta_page(req);
}

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err){
    char content[100] = {0};
    httpd_req_get_url_query_str(req, content, 100);
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, "file not found", HTTPD_RESP_USE_STRLEN);
    ESP_LOGE("http_404_error_handler", "Cannot find '%s'", content);

    return ESP_OK;


    // // Set status
    // httpd_resp_set_status(req, "302 Temporary Redirect");
    // // Redirect to the "/" root directory
    // httpd_resp_set_hdr(req, "Location", "/");
    // // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    // httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    // ESP_LOGI("http_404_error_handler", "Redirecting to root");
    // return ESP_OK;
}

httpd_handle_t start_webserver(void){

    // ! switch to POST
    const httpd_uri_t index_uri = {
        .uri = "/index",
        .method = HTTP_GET,
        .handler = index_handler
    };
    // ! switch to POST
    const httpd_uri_t new_sta_uri = {
        .uri = "/new_sta",
        .method = HTTP_GET,
        .handler = new_sta_handler
    };

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 1;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI("start_webserver", "Starting server on port: '%d'", config.server_port);
    if(httpd_start(&server, &config) == ESP_OK){
        // Set URI handlers
        ESP_LOGI("start_webserver", "Registering URI handlers");
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &new_sta_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

int web_ui_start(){
    internal_events = xEventGroupCreate();
    if(internal_events == NULL)
        return 1;

    server = start_webserver();
    return server == NULL ? 2 : 0;
}

void web_ui_stop(){
    if(server)
        httpd_stop(server);

    server = NULL;
    
    vEventGroupDelete(internal_events);
}

int web_ui_wait_for_credentials(){
    if(internal_events == NULL)
        return 1;

    xEventGroupWaitBits(internal_events, CREDENTIALS_READY, pdTRUE, pdFALSE, portMAX_DELAY);

    return 0;
}

// in case of failure, ssid will contain "No WiFi network saved" while pwd will be empty.
// ssid_len and pwd_len are always set accordingly
esp_err_t web_ui_get_credentials(uint8_t* ssid, size_t* ssid_len, uint8_t* pwd, size_t* pwd_len){
    esp_err_t err;
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("nvs", NVS_READWRITE, &nvs));

    err = nvs_get_blob(nvs, "wifi_ssid", ssid, ssid_len);
    if(err != ESP_OK){
        goto FAIL;
    }

    err = nvs_get_blob(nvs, "wifi_pwd", pwd, pwd_len);
    if(err != ESP_OK){
        goto FAIL;
    }

    return ESP_OK;

FAIL:

    const char* msg = "No WiFi network saved";
    *ssid_len = strlen((const char*)msg);// must be < 32
    memcpy(ssid, msg, *ssid_len);

    *pwd_len = 0;
    nvs_close(nvs);
    return err;
}
