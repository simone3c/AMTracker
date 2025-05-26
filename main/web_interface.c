#include "web_interface.h"
#include "esp_log.h"
#include "esp_check.h"

#include "my_wifi.h"

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
extern const char new_sta_start[] asm("_binary_new_sta_html_start");
extern const char new_sta_end[] asm("_binary_new_sta_html_end");
extern const char exit_html_start[] asm("_binary_bye_html_start");
extern const char exit_html_end[] asm("_binary_bye_html_end");

static EventGroupHandle_t internal_events = NULL;
static httpd_handle_t server = NULL;

// ! remember to free 'out' when not needed anymore
int create_error_div(const char* err_msg, size_t err_msg_len, char** out){
    static const char* error_div_style = "<div>";//"<div style=\"background-color: #f8d7da; color: #721c24; text-align: center;\">";

    int out_len = strlen(error_div_style) + err_msg_len + strlen("</div>") + 1;
    *out = malloc(out_len);

//    ESP_LOGE("create_error_div", "len: %i - *out: %s", out_len, !*out ? "NULL" : "aaaaaaaaaaaaaaaa");

    ESP_RETURN_ON_FALSE(*out, ESP_ERR_NO_MEM, "create_error_div", "error message allocation failed");

    memset(*out, 0, out_len);

    strcat(*out, error_div_style);
    strncat(*out, err_msg, err_msg_len);
    strcat(*out, "</div>");

    return 0;
}

esp_err_t new_sta_handler(httpd_req_t *req){
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

        // BIT0 is "credentials are ready"
        xEventGroupSetBits(internal_events, BIT0);

        return ESP_OK;
    }

    const uint32_t root_len = index_end - index_start;

    ESP_LOGI("root_get_handler", "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_start, root_len);

    return ESP_OK;
}

int get_credentials_from_nvs(){
    return 0;
}

int send_new_sta_page(httpd_req_t* req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, new_sta_start, new_sta_end - new_sta_start);

    return 0;
}

esp_err_t index_handler(httpd_req_t *req){

    #define KEY "method"
    #define VALUE_LEN 4

    char query[100] = {0};
    char value[VALUE_LEN] = {0};
    
    if(httpd_req_get_url_query_str(req, &query[0], 100) != ESP_OK){
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, index_start, index_end - index_start);
        
        return ESP_OK;
    }

    ESP_LOGI("root_get_handler", "QUERY: '%s'", query);

    // bad GET values, responding with index.html + an error message in the page
    if(httpd_query_key_value(query, KEY, &value[0], VALUE_LEN) != ESP_OK || 
        (strcmp(value, "NVS") && strcmp(value, "NEW"))
    ){

        static const char* error_msg = "Bad GET values";
        char* error_div;
        int t = create_error_div(error_msg, strlen(error_msg), &error_div);
        if(t != ESP_OK){
            
            return ESP_ERR_NO_MEM; 
        }
        
        uint16_t error_div_len = strlen(error_div);
        uint16_t index_len = index_end - index_start;
        uint16_t resp_len = index_len + error_div_len;

        char* resp = malloc(resp_len);
        ESP_RETURN_ON_FALSE(resp, ESP_ERR_NO_MEM, "index_hanlder", "response allocation failed 2");
        memset(resp, 0, resp_len);
        
        char* add_ptr = strstr(index_start, "<body>") + strlen("<body>"); // points to the end of <body> inside index
        int add_idx = add_ptr - index_start;

        memcpy(resp, index_start, add_idx); // cpy first part of index
        memcpy(resp + add_idx, error_div, error_div_len); // cpy error div
        memcpy(resp + add_idx + error_div_len, add_ptr, index_end - add_ptr); // cpy last part of index

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, resp, resp_len);

        free(error_div);
        free(resp);

        return ESP_OK;
    }

    if(strcmp(value, "NVS") == 0){
        get_credentials_from_nvs();
    }

    else{
        send_new_sta_page(req);
    }

    return ESP_OK;    
}

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err){
    char content[100];
    memset(content, 0, 100);
    httpd_req_get_url_query_str(req, content, 100);
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_send(req, "icon not found", HTTPD_RESP_USE_STRLEN);
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
    static const httpd_uri_t index_uri = {
        .uri = "/index",
        .method = HTTP_GET,
        .handler = index_handler
    };
    // ! switch to POST
    static const httpd_uri_t new_sta_uri = {
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

int start_web_interface(){
    internal_events = xEventGroupCreate();
    if(internal_events == NULL)
        return 1;

    server = start_webserver();
    return server == NULL ? 2 : 0;
}

void stop_web_interface(){
    if(server)
        httpd_stop(server);
    
    vEventGroupDelete(internal_events);
}

int wait_for_credentials(){
    if(internal_events == NULL)
        return 1;

    xEventGroupWaitBits(internal_events, BIT0, 0, 0, portMAX_DELAY); // should clear bit to block until new credentials are ready

    return 0;
}

