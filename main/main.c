#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <time.h>
#include "freertos/FreeRTOSConfig.h"
#include <esp_err.h>
#include <nvs_flash.h>
#include <string.h>
#include <inttypes.h>
#include "esp_spiffs.h"

#include "my_wifi.h"
#include "ntp_client.h"
#include "schedule.h"
#include "train.h"

#define MSEC(x) ((x) / portTICK_PERIOD_MS)

#define STOPS_NUM 8
#define TRAINS_NUM 801

#define DATA 14
#define LATCH 13
#define CLK 12

const line_t BRIN_BRIGNOLE = {
    .name = "brin_brignole",
    .path = {
        BRIN,
        DINEGRO,
        PRINCIPE,
        DARSENA,
        SANGIORGIO,
        SARZANO,
        DEFERRARI,
        BRIGNOLE,
        BRIN_DINEGRO,
        DINEGRO_PRINCIPE,
        PRINCIPE_DARSENA,
        DARSENA_SANGIORGO,
        SANGIORGIO_SARZANO,
        SARZANO_DEFERRARI,
        DEFERRARI_BIRGNOLE,
    },
    .stops_num = STOPS_NUM
};

const line_t BRIGNOLE_BRIN = {
    .name = "brignole_brin",
    .path = {
        BRIGNOLE,
        DEFERRARI,
        SARZANO,
        SANGIORGIO,
        DARSENA,
        PRINCIPE,
        DINEGRO,
        BRIN,
        DEFERRARI_BIRGNOLE,
        SARZANO_DEFERRARI,
        SANGIORGIO_SARZANO,
        DARSENA_SANGIORGO,
        PRINCIPE_DARSENA,
        DINEGRO_PRINCIPE,
        BRIN_DINEGRO,
    },
    .stops_num = STOPS_NUM
};

static train_t trains[TRAINS_NUM] = {0};

// TODO LED in case of error
int read_schedule(FILE* f){

    int idx = 0;
    uint8_t h, m, s;
    char* line = NULL;
    size_t line_len = 0;

    // csv description line
    __getline(&line, &line_len, f);

    while(idx < TRAINS_NUM){
        free(line);
        line = NULL;
        line_len = 0;

        int train_id_check = 0;

        for(int i = 0; i < STOPS_NUM; ++i){
            int stop_idx = 0;

            __getline(&line, &line_len, f);

            char* tokens[7];
            int tok_id = 0;
            char* aux = NULL;
            char* token = strtok_r(line, ",", &aux);
            while(token){
                tokens[tok_id++] = token;
                token = strtok_r(NULL, ",", &aux);
                //ESP_LOGI("read_schedule", "tokens[%i]: %s -- ", tok_id - 1, tokens[tok_id - 1]);
            }

            // id
            aux = NULL;
            int train_id = atoi(strtok_r(tokens[0], "-", &aux));

            if(!i)
                train_id_check = train_id;
            // CSV is not correctly sorted
            else if(train_id != train_id_check)
                return 2;

            // day
            strtok_r(NULL, "-", &aux);
            strtok_r(NULL, "-", &aux);
            char* period_str = strtok_r(NULL, "-", &aux);
            day_t day = SUNDAY;
            if(!strcmp(period_str, "Semaine")){
                day = MON_FRI;
            }
            else if(!strcmp(period_str, "Samedi"))
                day = SATURDAY;

            trains[idx].id = train_id;
            trains[idx].day = day;

            // stop index
            stop_idx = atoi(tokens[4]) - 1;
            if(stop_idx > 7)
                return 1;

            // arrival
            aux = NULL;
            h = atoi(strtok_r(tokens[1], ":", &aux));
            m = atoi(strtok_r(NULL, ":", &aux));
            s = atoi(strtok_r(NULL, ":", &aux));
            trains[idx].arrival[stop_idx] = (schedule_t){h, m, s};

            // departure
            aux = NULL;
            h = atoi(strtok_r(tokens[2], ":", &aux));
            m = atoi(strtok_r(NULL, ":", &aux));
            s = atoi(strtok_r(NULL, ":", &aux));
            trains[idx].departure[stop_idx] = (schedule_t){h, m, s};

            // line
            if(stop_idx == 0){
                trains[idx].line = !strcmp(tokens[3], "MM08") ? &BRIGNOLE_BRIN : &BRIN_BRIGNOLE;
            }
        }

        ++idx;
    }

    return 0;
}

void get_active_trains(const schedule_t* now_sched, day_t day, train_t** active_trains, size_t* len, size_t max_size){

    memset(active_trains, 0, max_size);
    *len = 0;

    for(int i = 0; i < TRAINS_NUM; ++i){

        if(trains[i].day != day)
            continue;

        if(is_between(&trains[i].arrival[0], &trains[i].departure[STOPS_NUM - 1], *now_sched)){
            //ESP_LOGI("get_active_trains", "found an active train: %i", trains[i].id);
            active_trains[(*len)++] = &trains[i];
        }
    }
}

void calc_train_position(const train_t* ptr, const schedule_t* now_sched, train_position_t* train_pos){

    memset(train_pos, 0, sizeof(train_position_t));

    // check if train is stopped in a station
    for(int j = 0; j < STOPS_NUM; ++j){
        if(is_between(&ptr->arrival[j], &ptr->departure[j], *now_sched)){
            train_pos->pos = ptr->line->path[j];
            train_pos->perc = 0.;
            return;
        }
    }

    // check if train is moving between 2 stations
    for(int j = 0; j < STOPS_NUM - 1; ++j){
        if(is_between(&ptr->departure[j], &ptr->arrival[j + 1], *now_sched)){
            train_pos->pos = ptr->line->path[j + STOPS_NUM];
            train_pos->perc = get_percentage(&ptr->departure[j], &ptr->arrival[j + 1], *now_sched);
        }
    }

}

void update_position(){

    // TODO adust array size
    #define max_active_trains 32
    train_t* active_trains[max_active_trains];
    size_t active_trains_len = 0;

    train_position_t train_pos[max_active_trains] = {0};

    while(4){

        const time_t now_seconds = time(0);
        struct tm now;
        localtime_r(&now_seconds, &now);
        schedule_t now_sched = {now.tm_hour, now.tm_min, now.tm_sec};
        //now_sched = (schedule_t){0, 55, 21};

        ESP_LOGI("update_position", "now is: %i - %i - %i", now_sched.hour, now_sched.min, now_sched.sec);

        day_t day = MON_FRI;
            if(now.tm_wday == 6)
                day = SATURDAY;
            else if(!now.tm_wday)
                day = SUNDAY;

        get_active_trains(&now_sched, day, active_trains, &active_trains_len, max_active_trains);

        for(int i = 0; i < active_trains_len; ++i){
            //print_train(active_trains[i]);
            calc_train_position(active_trains[i], &now_sched, &train_pos[i]);
            ESP_LOGI("update_position", "position of train [%i] is %i @ %.2f%%", active_trains[i]->id, train_pos[i].pos, 100 * train_pos[i].perc);
        }

        // send positions and perc to a queue to be printed on LED


        vTaskDelay(MSEC(2000));

    }

}

void shift_write(uint8_t data){
    gpio_set_level(LATCH, 0);
    gpio_set_level(CLK, 0);
    for(int i = 0; i < 8; ++i){
        gpio_set_level(DATA, data & (1 << i));

        gpio_set_level(CLK, 1);
        gpio_set_level(CLK, 0);

    }
    gpio_set_level(LATCH, 1);
    gpio_set_level(LATCH, 0);
}

void draw_position(){




}

void app_main(void){
    gpio_config_t pin_cfg = {
        .pin_bit_mask = (1 << DATA) | (1 << CLK) | (1 << LATCH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pin_cfg));

    int a = 1;
    int t = 10;
    uint8_t data = 0b01100110;
    while(4){

        shift_write(data);
        return;

        vTaskDelay(MSEC(t));
        shift_write(0);
        vTaskDelay(MSEC(t));


    }

/*
    setup_wifi();
    if(get_ntp_clock())
    // set led error and exit
        ESP_LOGE("main", "ERROR NTP");

    const esp_vfs_spiffs_conf_t spiffs_cfg = {
        .base_path = "/spiffs_root",
        .partition_label = NULL,
        .max_files = 1,
        .format_if_mount_failed = false
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_cfg));

    FILE* t = fopen("/spiffs_root/stop_fixed.csv", "r");

    if(t == NULL){
        ESP_LOGE("main", "file: %s", strerror(errno));
        exit(0);
    }

    int err;
    if((err = read_schedule(t)))
        //turn on error LED and exit
        ESP_LOGE("main", "ERROR READ SCHEDULE: %i", err);

    update_position();
*/

}
