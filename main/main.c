#include <stdio.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "my_wifi.h"
#include "ntp_client.h"
#include "schedule.h"
#include "train.h"
#include "sn74hc595.h"

#define MSEC(x) ((x) / portTICK_PERIOD_MS)

#define STOPS_NUM 8
#define TRAINS_NUM 801
#define MAX_ACTIVE_TRAINS 32

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
const led_t LEDS[] = {
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

// encode the matrix content in 8 bytes
typedef uint64_t matrix_queue_elem_t;

QueueHandle_t matrix_queue;

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
        DARSENA_SANGIORGIO,
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
        DARSENA_SANGIORGIO,
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

    memset(train_pos, 0, sizeof(*train_pos));

    train_pos->train = ptr;

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

// TODO data size
void matrix_write(uint8_t row, uint8_t col){
    uint8_t data[] = {col, row};
    sn74hc595_write(data, 2, false);
}

// roe is between 0 and 7 - col is between 0 and 7
void matrix_draw_line(uint8_t row, uint8_t col){
    // bit-wise not is needed because the matrix is in common anode
    matrix_write(1 << row, ~col);
}

// todo maybe transform in a lookup table
uint8_t checkpoint_to_display_coord(checkpoint_t c){

    switch(c){
        case BRIN:
        case BRIN_DINEGRO:
            return 1;

        case DINEGRO:
        case DINEGRO_PRINCIPE:
            return 1 << 1;

        case PRINCIPE:
        case PRINCIPE_DARSENA:
            return 1 << 2;

        case DARSENA:
        case DARSENA_SANGIORGIO:
            return 1 << 3;

        case SANGIORGIO:
        case SANGIORGIO_SARZANO:
            return 1 << 4;

        case SARZANO:
        case SARZANO_DEFERRARI:
            return 1 << 5;

        case DEFERRARI:
        case DEFERRARI_BIRGNOLE:
            return 1 << 6;

        case BRIGNOLE:
            return 1 << 7;

        default:
            ESP_LOGE("shader", "is_station case -- unreachable!!");
    }

    return 0;
}

void shader(train_position_t* active_trains, size_t sz, matrix_queue_elem_t* out){
    *out = 0;
    for(int i = 0; i < sz; ++i){
        uint8_t row = 10, col = 10;
        ESP_LOGI("shader", "id: %i", active_trains[i].train->id);

        uint8_t len;
        switch (active_trains[i].pos){
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
            len = 0;
            break;
        }

        uint8_t id = active_trains[i].perc * len;
        checkpoint_t dest = active_trains[i].train->line->path[active_trains[i].train->line->stops_num - 1];
        checkpoint_t pos = active_trains[i].pos;

        for(int j = 0; j < sizeof(LEDS) / sizeof(LEDS[0]); ++j){
            if(LEDS[j].dest == dest
                && LEDS[j].pos == pos
                && LEDS[j].relative_id == id
            ){
                row = LEDS[j].row;
                col = LEDS[j].col;
                break;
            }
        }
        
        ESP_LOGI("shader", "row: %"PRIu8" - col: %"PRIu8" - id: %"PRIu8, row, col, id);

        *out |= ((uint64_t)1 << col) << (8 * row);
    }
}

static bool draw(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void* args){

    static matrix_queue_elem_t data = 0;
    static uint8_t row = 0;

    xQueuePeekFromISR(matrix_queue, &data);

    matrix_draw_line(row, (data >> (8 * row)) & 0xFF);
    
    if(++row > 7)
        row = 0;

    return 1;
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

    matrix_queue = xQueueCreate(1, sizeof(matrix_queue_elem_t));

    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz
        .intr_priority = 0,
        .flags = {0},
    };
    gptimer_handle_t timer;
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &timer));

    gptimer_alarm_config_t timer_alarm = {
        .alarm_count = 2000, // 2ms period with 1MHz timer freq (emprical value)
        .reload_count = 0,
        .flags = {
            .auto_reload_on_alarm = true
        }
    };

    gptimer_event_callbacks_t cb = {
        .on_alarm = draw
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &timer_alarm));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &cb, NULL));

    ESP_ERROR_CHECK(gptimer_enable(timer));
    gptimer_start(timer);


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

   
    while(4){
        
        train_t* active_trains[MAX_ACTIVE_TRAINS] = {0};
        size_t active_trains_len = 0;
        train_position_t train_pos[MAX_ACTIVE_TRAINS] = {0};


        const time_t now_seconds = time(0);
        struct tm now;
        localtime_r(&now_seconds, &now);
        schedule_t now_sched = {now.tm_hour, now.tm_min, now.tm_sec};
        //now_sched = (schedule_t){18, 26, 21};

        ESP_LOGI("main", "now is: %i - %i - %i", now_sched.hour, now_sched.min, now_sched.sec);

        day_t day = MON_FRI;
        if(now.tm_wday == 6)
            day = SATURDAY;
        else if(!now.tm_wday)
            day = SUNDAY;

        get_active_trains(&now_sched, day, active_trains, &active_trains_len, MAX_ACTIVE_TRAINS);

        for(int i = 0; i < active_trains_len; ++i){
            //print_train(active_trains[i]);
            calc_train_position(active_trains[i], &now_sched, &train_pos[i]);
            ESP_LOGI("main", "position of train [%i] running on %s is %i @ %.2f%%", 
                train_pos[i].train->id, 
                train_pos[i].train->line->name,
                train_pos[i].pos, 
                100 * train_pos[i].perc                
            );
        }

        // convert from station and percentage to rows and columns of the LED matrix
        matrix_queue_elem_t matrix_data;
        shader(train_pos, active_trains_len, &matrix_data);
        xQueueOverwrite(matrix_queue, &matrix_data);

        vTaskDelay(MSEC(2000));
    }
    
}
