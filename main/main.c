/*

    ! GTFS's support for >24 hour format (i.e. 01:00 == 25:00) and so does this project

*/

#include <time.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_spiffs.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "lwip/ip_addr.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "my_wifi.h"
#include "schedule.h"
#include "train.h"
#include "sn74hc595.h"
#include "led_matrix.h"
#include "csv_reader.h"
#include "my_err.h"
#include "web_ui.h"

#define USE_WIFI

static gptimer_handle_t timer = NULL;

#define MSEC(x) ((x) / portTICK_PERIOD_MS)

#define STOPS_NUM 8
#define TRAINS_NUM 801

#define SLEEP_DEFAULT_S 15

#define DATA 14
#define CLK 13
#define LATCH 12

void sn74hc595_set_clk_pin(bool v){
    gpio_set_level(CLK, v);
}
void sn74hc595_set_data_pin(bool v){
    gpio_set_level(DATA, v);
}
void sn74hc595_set_latch_pin(bool v){
    gpio_set_level(LATCH, v);
}

typedef struct{
    uint8_t row;
    uint8_t col;
    uint8_t relative_id;
    checkpoint_t pos;
    checkpoint_t dest;
} led_t;
// encode the matrix content in 8 bytes
typedef uint64_t matrix_queue_elem_t;
QueueHandle_t matrix_queue;

const line_t BRIN_BRIGNOLE = {
    .name = "brin_brignole",
    .path = {
        BRIN,
        BRIN_DINEGRO,
        DINEGRO,
        DINEGRO_PRINCIPE,
        PRINCIPE,
        PRINCIPE_DARSENA,
        DARSENA,
        DARSENA_SANGIORGIO,
        SANGIORGIO,
        SANGIORGIO_SARZANO,
        SARZANO,
        SARZANO_DEFERRARI,
        DEFERRARI,
        DEFERRARI_BIRGNOLE,
        BRIGNOLE,
    },
    .stops_num = STOPS_NUM
};
const line_t BRIGNOLE_BRIN = {
    .name = "brignole_brin",
    .path = {
        BRIGNOLE,
        DEFERRARI_BIRGNOLE,
        DEFERRARI,
        SARZANO_DEFERRARI,
        SARZANO,
        SANGIORGIO_SARZANO,
        SANGIORGIO,
        DARSENA_SANGIORGIO,
        DARSENA,
        PRINCIPE_DARSENA,
        PRINCIPE,
        DINEGRO_PRINCIPE,
        DINEGRO,
        BRIN_DINEGRO,
        BRIN,
    },
    .stops_num = STOPS_NUM
};

static wifi_config_t sta_cfg = {
    .sta = {
        .ssid = {0},
        .password = {0},
        .threshold.authmode = WIFI_AUTH_OPEN,
        .pmf_cfg.required = false, // security feature
    },
};

// assumes that lines relative to the same train come back-to-back
my_err_t read_schedule(train_t* trains, const char* file){

    size_t idx = 0;
    uint8_t h, m, s;
    csv_line_t line;

    csv_reader_t csv;
    if(csv_reader_init(&csv, file, ',') < 0)
        return CSV_CANNOT_OPEN_FILE;

    while(idx < TRAINS_NUM){

        int train_id_check = 0;
        int train_id;
        int stop_idx;

        for(int i = 0; i < STOPS_NUM; ++i){

            if(csv_reader_getline(&csv, &line))
                return PARSER_GETLINE;

            char* token = NULL;
            char* useless;

            // train ID
            if(csv_reader_getfield(&csv, &line, "trip_id", &token) < 0)
                return CSV_UNKNOWN_FIELD;

            train_id = strtol(token, &useless, 10);
            if(i && train_id != train_id_check){
                free(token);
                csv_reader_line_free(&line);
                csv_reader_deinit(&csv);
                return CSV_WRONG_ORDER;
            }

            train_id_check = train_id;

            // stop index
            free(token);
            if(csv_reader_getfield(&csv, &line, "stop_sequence", &token) < 0)
                return CSV_UNKNOWN_FIELD;
            stop_idx = strtol(token, &useless, 10) - 1;

            // arrival
            free(token);
            char* aux = NULL;
            if(csv_reader_getfield(&csv, &line, "arrival_time", &token) < 0)
                return CSV_UNKNOWN_FIELD;
            h = atoi(strtok_r(token, ":", &aux));
            m = atoi(strtok_r(NULL, ":", &aux));
            s = atoi(strtok_r(NULL, ":", &aux));
            trains[idx].arrival[stop_idx] = (schedule_t){h, m, s};

            // departure
            free(token);
            aux = NULL;
            if(csv_reader_getfield(&csv, &line, "departure_time", &token) < 0)
                return CSV_UNKNOWN_FIELD;
            h = atoi(strtok_r(token, ":", &aux));
            m = atoi(strtok_r(NULL, ":", &aux));
            s = atoi(strtok_r(NULL, ":", &aux));
            trains[idx].departure[stop_idx] = (schedule_t){h, m, s};

            // line
            free(token);
            if(csv_reader_getfield(&csv, &line, "stop_id", &token) < 0)
                return CSV_UNKNOWN_FIELD;
            if(!i)
                trains[idx].line = !strcmp(token, "MM03") ? &BRIGNOLE_BRIN : &BRIN_BRIGNOLE;

            // day
            free(token);
            if(csv_reader_getfield(&csv, &line, "day", &token) < 0)
                return CSV_UNKNOWN_FIELD;
            char* day_str = token;
            day_t day = SUNDAY;
            if(!strcmp(day_str, "Mon_Fri"))
                day = MON_FRI;
            else if(!strcmp(day_str, "Sat"))
                day = SATURDAY;

            trains[idx].id = train_id;
            trains[idx].day = day;

            free(token);
            csv_reader_line_free(&line);
        }

        ++idx;
    }

    csv_reader_deinit(&csv);

    return OK;
}

my_err_t train_to_led(const train_t* train, uint8_t* row, uint8_t* col, uint8_t* id_ret){
    // range [0, 7] for leds coordinates
    static const led_t LEDS[] = {
        // stations
        {0, 0, 0, BRIN, BRIGNOLE},
        {0, 1, 0, DINEGRO, BRIGNOLE},
        {0, 2, 0, PRINCIPE, BRIGNOLE},
        {0, 3, 0, DARSENA, BRIGNOLE},
        {0, 4, 0, SANGIORGIO, BRIGNOLE},
        {0, 5, 0, SARZANO, BRIGNOLE},
        {0, 6, 0, DEFERRARI, BRIGNOLE},
        {0, 7, 0, BRIGNOLE, BRIGNOLE},
        // intermediate points
        {1, 0, 0, BRIN_DINEGRO, BRIGNOLE},
        {1, 1, 1, BRIN_DINEGRO, BRIGNOLE},
        {1, 2, 2, BRIN_DINEGRO, BRIGNOLE},
        {1, 3, 3, BRIN_DINEGRO, BRIGNOLE},
        {1, 4, 4, BRIN_DINEGRO, BRIGNOLE},
        {1, 5, 5, BRIN_DINEGRO, BRIGNOLE},
        {1, 6, 0, DINEGRO_PRINCIPE, BRIGNOLE},
        {1, 7, 1, DINEGRO_PRINCIPE, BRIGNOLE},
        {2, 0, 0, PRINCIPE_DARSENA, BRIGNOLE},
        {2, 1, 1, PRINCIPE_DARSENA, BRIGNOLE},
        {2, 2, 0, DARSENA_SANGIORGIO, BRIGNOLE},
        {2, 3, 1, DARSENA_SANGIORGIO, BRIGNOLE},
        {2, 4, 0, SANGIORGIO_SARZANO, BRIGNOLE},
        {2, 5, 1, SANGIORGIO_SARZANO, BRIGNOLE},
        {2, 6, 0, SARZANO_DEFERRARI, BRIGNOLE},
        {2, 7, 1, SARZANO_DEFERRARI, BRIGNOLE},
        {3, 0, 0, DEFERRARI_BIRGNOLE, BRIGNOLE},
        {3, 1, 1, DEFERRARI_BIRGNOLE, BRIGNOLE},
        {3, 2, 2, DEFERRARI_BIRGNOLE, BRIGNOLE},

        // stations
        {7, 0, 0, BRIN, BRIN},
        {7, 1, 0, DINEGRO, BRIN},
        {7, 2, 0, PRINCIPE, BRIN},
        {7, 3, 0, DARSENA, BRIN},
        {7, 4, 0, SANGIORGIO, BRIN},
        {7, 5, 0, SARZANO, BRIN},
        {7, 6, 0, DEFERRARI, BRIN},
        {7, 7, 0, BRIGNOLE, BRIN},
        // intermediate points
        {6, 0, 0, DEFERRARI_BIRGNOLE, BRIN},
        {6, 1, 1, DEFERRARI_BIRGNOLE, BRIN},
        {6, 2, 2, DEFERRARI_BIRGNOLE, BRIN},
        {6, 3, 0, SARZANO_DEFERRARI, BRIN},
        {6, 4, 1, SARZANO_DEFERRARI, BRIN},
        {6, 5, 0, SANGIORGIO_SARZANO, BRIN},
        {6, 6, 1, SANGIORGIO_SARZANO, BRIN},
        {6, 7, 0, DARSENA_SANGIORGIO, BRIN},
        {5, 0, 1, DARSENA_SANGIORGIO, BRIN},
        {5, 1, 0, PRINCIPE_DARSENA, BRIN},
        {5, 2, 1, PRINCIPE_DARSENA, BRIN},
        {5, 3, 0, DINEGRO_PRINCIPE, BRIN},
        {5, 4, 1, DINEGRO_PRINCIPE, BRIN},
        {5, 5, 0, BRIN_DINEGRO, BRIN},
        {5, 6, 1, BRIN_DINEGRO, BRIN},
        {5, 7, 2, BRIN_DINEGRO, BRIN},
        {4, 0, 3, BRIN_DINEGRO, BRIN},
        {4, 1, 4, BRIN_DINEGRO, BRIN},
        {4, 2, 5, BRIN_DINEGRO, BRIN},
    };

    uint8_t len;
    // number of LEDs for each intermediate section (between 2 stations)
    switch (train->status.position.checkpoint){
    case DINEGRO_PRINCIPE:
    case PRINCIPE_DARSENA:
    case DARSENA_SANGIORGIO:
    case SANGIORGIO_SARZANO:
    case SARZANO_DEFERRARI:
        len = 2;
        break;
    case BRIN_DINEGRO:
        len = 6;
        break;
    case DEFERRARI_BIRGNOLE:
        len = 3;
        break;
    default:
        // train is in station
        len = 0;
        break;
    }

    uint8_t id = train->status.position.perc * len;
    if(len > 0 && id == len)
        --id; // avoid out-of-range index
    checkpoint_t dest = train->line->path[2 * train->line->stops_num - 1];
    checkpoint_t pos = train->status.position.checkpoint;

    ESP_LOGI("train_to_led", "id: %"PRIu8" - dest: %d - pos: %d", id, dest, pos);

    for(int i = 0; i < sizeof(LEDS) / sizeof(LEDS[0]); ++i){
        if(LEDS[i].dest == dest
            && LEDS[i].pos == pos
            && LEDS[i].relative_id == id
        ){
            *row = LEDS[i].row;
            *col = LEDS[i].col;
            *id_ret = id;

            return OK;
        }
    }

    return CANNOT_CONVERT_TO_LED;
}

// for limiting current and because of matrix's structure (common anode),
// the matrix is drawn one line at a time
static bool ISR_draw(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void* args){
    static uint8_t row = 0;
    matrix_queue_elem_t data = 0;
    uint8_t cols;

    xQueuePeekFromISR(matrix_queue, &data);

    // row-th byte represents the row-th column
    cols = (data >> (8 * row)) & 0xFF;

    matrix_draw(1 << row, cols);

    if(++row > 7)
        row = 0;

    return 1;
}

my_err_t get_ntp_clock(){

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("0.it.pool.ntp.org");

    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

    if(esp_netif_sntp_sync_wait(1000) != ESP_OK)
        return SNTP_SYNC_FAIL;

    esp_netif_sntp_deinit();

    return OK;
}

void gpio_init(){
    // GPIO config
    gpio_config_t pin_cfg = {
        .pin_bit_mask = (1 << DATA) | (1 << CLK) | (1 << LATCH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pin_cfg));
}

void timer_init(){
    // ISR led matrix timer config
    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz
        .intr_priority = 0,
        .flags = {0},
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &timer));
    gptimer_alarm_config_t timer_alarm = {
        .alarm_count = 2000, // 2ms period with 1MHz timer freq (empirical value)
        .reload_count = 0,
        .flags = {
            .auto_reload_on_alarm = true
        }
    };
    gptimer_event_callbacks_t cb = {
        .on_alarm = ISR_draw
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &timer_alarm));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &cb, NULL));
    ESP_ERROR_CHECK(gptimer_enable(timer));
}

// ! TEST
my_err_t handle_deepsleep_reset(){

    size_t ssid_len = 32;
    size_t pwd_len = 64;

    ESP_ERROR_CHECK(web_ui_get_credentials(&sta_cfg.sta.ssid[0], &ssid_len, &sta_cfg.sta.password[0], &pwd_len));
    if(sta_cfg.sta.password[0] == 0){
        return NO_CREDENTIALS_AVAILABLE;
    }

    if(wifi_apsta_connect_to(&sta_cfg) != ESP_OK){
        wifi_stop_and_deinit();
        esp_deep_sleep(60 * 10 * 1000000); // sleep 10 min
    }

    return ESP_OK;
}

// ! correctly initialise nvs(?) before this, otherwise it doesnt mount the partition
// ! (it works only after wifi and sntp set up something (nvs?))
void spiffs_init(){
    const esp_vfs_spiffs_conf_t spiffs_cfg = {
        .base_path = "/spiffs_root",
        .partition_label = NULL,
        .max_files = 1,
        .format_if_mount_failed = false
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_cfg));
}

void nvs_init(){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void web_ui_run(){
    size_t ssid_len = MAX_SSID_LEN;
        size_t pwd_len = MAX_PASSPHRASE_LEN;

    web_ui_start();

    while(4){
        if(web_ui_wait_for_credentials() != 0){
            ESP_LOGE("main", "cannot wait for credentials if webUI is not initialised!!");
            assert(false);
        }

        if(web_ui_get_credentials(&sta_cfg.sta.ssid[0], &ssid_len, &sta_cfg.sta.password[0], &pwd_len) != ESP_OK)
            ESP_LOGI("main", "error while retrieving credentials");

        // sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        if(wifi_apsta_connect_to(&sta_cfg) == ESP_OK){ // connection established
            web_ui_notify_connection_established();
            break;
        }

        ESP_LOGI("main", "wrong credentials");
        // communicate with webui to ask credentials again
        web_ui_notify_connection_failed();

    }

    web_ui_stop();
}

void app_main(void){

    static train_t trains[TRAINS_NUM] = {0};

    matrix_queue = xQueueCreate(1, sizeof(matrix_queue_elem_t));

    // first train of each period
    // [0] is SUNDAY
    // [1] is MON_FRI
    // [2] is SATURDAY
    // initialisation is useful when filling the array
    schedule_t first_train[3] = {
        {100, 100, 100}, // SUNDAY
        {100, 100, 100}, // MON_FRI
        {100, 100, 100}, // SATURDAY
    };
    schedule_t last_train[3] = {0};

    const wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "esp32",
            .ssid_len = strlen("esp32"),
            .password = "",
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg.required = false,
        },
    };

#ifdef USE_WIFI
    nvs_init();
    ESP_ERROR_CHECK(wifi_apsta_setup(&sta_cfg, &ap_cfg));
    ESP_ERROR_CHECK(wifi_start());

    my_err_t cred_status = NO_CREDENTIALS_AVAILABLE;

    if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER){
        cred_status = handle_deepsleep_reset();
    }

    if(cred_status == NO_CREDENTIALS_AVAILABLE){
        web_ui_run();
    }
    else{
        if(wifi_apsta_connect_to(&sta_cfg) != ESP_OK){
            ESP_LOGI("main", "wrong credentials");
            web_ui_run();
        }
    }

    // TODO better error handling
    if(get_ntp_clock())
        ESP_LOGE("main", "ERROR NTP");

    wifi_stop_and_deinit();

#endif

    gpio_init();
    timer_init();
    spiffs_init();

    my_err_t err = read_schedule(trains, "/spiffs_root/stop_times_fixed.csv");
    if(err != OK){
        // TODO turn on error LED and exit
        ESP_LOGE("main", "ERROR READ SCHEDULE: %s - errno: %s", strerr(err), strerror(errno));

    }

    // find first and last train of each day
    for(int i = 0; i < TRAINS_NUM; ++i){
        int idx;
        switch(trains[i].day){
        case SUNDAY:
            idx = 0;
            break;

        case MON_FRI:
            idx = 1;
            break;

        case SATURDAY:
            idx = 2;
            break;

        default:
            assert(false);
        }

        if(schedule_cmp(&trains[i].arrival[0], &first_train[idx]) == -1)
            first_train[idx] = trains[i].arrival[0];

        else if(schedule_cmp(&trains[i].arrival[0], &last_train[idx]) == 1)
            last_train[idx] = trains[i].arrival[0];
    }

    // start drawing LED matrix
    ESP_ERROR_CHECK(gptimer_start(timer));

    while(4){

        const time_t now_seconds = time(0);
        struct tm now;
        localtime_r(&now_seconds, &now);
        schedule_t now_sched = {now.tm_hour, now.tm_min, now.tm_sec};
        day_t today = now.tm_wday;

#ifndef USE_WIFI
        // set to a preferred time for testing purposes
        now_sched = (schedule_t){18, 0, 10};
        today = MON_FRI;
#endif
        // chneg the current day at 4 when no trains are running
        if(now_sched.hour < 4){
            now_sched.hour += 24;
            today = today == SUNDAY ? SATURDAY : today - 1; // trick!!
        }

        ESP_LOGI("main", "now is: %i - %i - %i - %i", today, now_sched.hour, now_sched.min, now_sched.sec);

        matrix_queue_elem_t led_matrix = 0;

        uint8_t active_trains = 0;

        for(int i = 0; i < TRAINS_NUM; ++i){

            update_train_status(&trains[i], &now_sched, today);

            if(trains[i].status.is_active){

                ++active_trains;

                uint8_t row, col, id;
                my_err_t err = train_to_led(&trains[i], &row, &col, &id);
                // convert from checkpoint and percentage to rows and columns of the LED matrix
                if(err == OK)
                    led_matrix |= ((uint64_t)1 << col) << (8 * row);
                else
                    ESP_LOGE("main", "%s", strerr(err));

                ESP_LOGI("main", "train [%i] line [%s] now in [%s] @ %.2f%% | row: %"PRIu8" - col: %"PRIu8" - id: %"PRIu8,
                    trains[i].id,
                    trains[i].line->name,
                    checkpoint_str(trains[i].status.position.checkpoint),
                    100 * trains[i].status.position.perc,
                    row,
                    col,
                    id
                );
            }
        }

        xQueueOverwrite(matrix_queue, &led_matrix);

        if(!active_trains){

            uint8_t id_last, id_first;

            // init required in case I'm here but there are more trains today:
            // I'm not going to deep sleep but just delay this task for SLEEP_DEFAULT_S
            uint16_t sleep_time_s = SLEEP_DEFAULT_S;

            // indices to search for the next train
            switch(today){
            case SUNDAY:
                id_last = 0;
                id_first = 1;
                break;

            case FRIDAY:
                id_last = 1;
                id_first = 2;
                break;

            case SATURDAY:
                id_last = 2;
                id_first = 0;
                break;

            default:
                id_last = 1;
                id_first = 1;
                break;
            }

            if(schedule_cmp(&now_sched, &last_train[id_last]) == 1)
                sleep_time_s = schedule_diff(&now_sched, &first_train[id_first]);

            ESP_LOGI("main", "sleep time is: %us", sleep_time_s);

            if(sleep_time_s > 60)
                esp_deep_sleep(sleep_time_s * 1000000);

            if(sleep_time_s >= SLEEP_DEFAULT_S)
                vTaskDelay(MSEC(sleep_time_s * 1000));
        }

        vTaskDelay(MSEC(2000));
    }
}
