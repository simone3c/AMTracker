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
#include "esp_sleep.h"
#include "esp_spiffs.h"

#include "my_wifi.h"
#include "ntp_client.h"
#include "schedule.h"
#include "train.h"
#include "sn74hc595.h"
#include "led_matrix.h"
#include "csv_reader.h"

#define MSEC(x) ((x) / portTICK_PERIOD_MS)

#define STOPS_NUM 8
#define TRAINS_NUM 801

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

typedef enum{
    OK,
    PARSER_GETLINE,
    TOO_MANY_TRAINS,
    CSV_LINE_FORMAT,
    CSV_ORDER,


} internal_err_t;

// assumes that lines relative to the same train come back-to-back
internal_err_t read_schedule(train_t* trains){

    size_t idx = 0;
    uint8_t h, m, s;
    csv_line_t line;

    csv_reader_t csv;
    int err = csv_reader_init(&csv, "/spiffs_root/stop_times_fixed.csv", ',');

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
            csv_reader_getfield(&csv, &line, "trip_id", &token);
            train_id = strtol(token, &useless, 10);
            // CSV is not correctly sorted
            if(i && train_id != train_id_check)
                return CSV_ORDER;
            
            train_id_check = train_id;
            
            // stop index
            free(token);
            csv_reader_getfield(&csv, &line, "stop_sequence", &token);
            stop_idx = strtol(token, &useless, 10) - 1;

            // arrival
            free(token);
            char* aux = NULL;
            csv_reader_getfield(&csv, &line, "arrival_time", &token);
            h = atoi(strtok_r(token, ":", &aux));
            m = atoi(strtok_r(NULL, ":", &aux));
            s = atoi(strtok_r(NULL, ":", &aux));
            trains[idx].arrival[stop_idx] = (schedule_t){h, m, s};
            
            // departure
            free(token);
            aux = NULL;
            csv_reader_getfield(&csv, &line, "departure_time", &token);
            h = atoi(strtok_r(token, ":", &aux));
            m = atoi(strtok_r(NULL, ":", &aux));
            s = atoi(strtok_r(NULL, ":", &aux));
            trains[idx].departure[stop_idx] = (schedule_t){h, m, s};

            // line
            free(token);
            csv_reader_getfield(&csv, &line, "stop_id", &token);
            if(!i)
                trains[idx].line = !strcmp(token, "MM03") ? &BRIGNOLE_BRIN : &BRIN_BRIGNOLE;
            
            // day
            free(token);
            csv_reader_getfield(&csv, &line, "day", &token);
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

void train_to_led(const train_t* train, uint8_t* row, uint8_t* col){

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

    *row = 255;
    *col = 255;

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
    if(id == len)
        --id; // avoid out-of-range index
    checkpoint_t dest = train->line->path[train->line->stops_num - 1];
    checkpoint_t pos = train->status.position.checkpoint;

    for(int j = 0; j < sizeof(LEDS) / sizeof(LEDS[0]); ++j){
        if(LEDS[j].dest == dest
            && LEDS[j].pos == pos
            && LEDS[j].relative_id == id
        ){
            *row = LEDS[j].row;
            *col = LEDS[j].col;
            break;
        }
    }   

    ESP_LOGI("train_to_led", "train [%i] running on %s is at %s @ %.2f%% | row: %"PRIu8" - col: %"PRIu8" - id: %"PRIu8, 
        train->id,
        train->line->name,
        checkpoint_str(train->status.position.checkpoint), 
        100 * train->status.position.perc,
        *row,
        *col,
        id               
    );

    assert(*row != 255 && *col != 255);
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

void app_main(void){

    static train_t trains[TRAINS_NUM] = {0};

    matrix_queue = xQueueCreate(1, sizeof(matrix_queue_elem_t));
    
    // GPIO config
    gpio_config_t pin_cfg = {
        .pin_bit_mask = (1 << DATA) | (1 << CLK) | (1 << LATCH),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pin_cfg));
     
    // ISR led matrix timer config 
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
    gptimer_start(timer);

    setup_wifi();

    // TODO turn on error LED and exit
    if(get_ntp_clock())
        ESP_LOGE("main", "ERROR NTP");

    wifi_stop();

    const esp_vfs_spiffs_conf_t spiffs_cfg = {
        .base_path = "/spiffs_root",
        .partition_label = NULL,
        .max_files = 1,
        .format_if_mount_failed = false
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_cfg));

    
    internal_err_t err = read_schedule(trains);
    if(err != OK)
        // TODO turn on error LED and exit
        ESP_LOGE("main", "ERROR READ SCHEDULE: %i", err);

    // idx 0 is SUNDAY
    // idx 1 is MON_FRI
    // idx 2 is SATURDAY
    schedule_t first_train[3] = {
        {100, 100, 100},
        {100, 100, 100},
        {100, 100, 100},
    };
    schedule_t last_train[3] = {0};

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

    while(4){
        
        const time_t now_seconds = time(0);
        struct tm now;
        localtime_r(&now_seconds, &now);
        schedule_t now_sched = {now.tm_hour, now.tm_min, now.tm_sec};
        day_t today = now.tm_wday;

        ESP_LOGI("main", "now is: %i - %i - %i - %i", today, now_sched.hour, now_sched.min, now_sched.sec);

        // set to a preferred time for testing purposes
        // now_sched = (schedule_t){0, 16, 21};
        // today = SATURDAY;

        // chneg the current day at 4 when no trains are running
        if(now_sched.hour < 4){
            now_sched.hour += 24;
            today = today == SUNDAY ? SATURDAY : today - 1; // trick!!
        }

        ESP_LOGI("main", "now in format H27 is: %i - %i - %i - %i", today, now_sched.hour, now_sched.min, now_sched.sec);

        matrix_queue_elem_t led_matrix = 0;

        uint8_t active_trains = 0;

        for(int i = 0; i < TRAINS_NUM; ++i){

            update_train_status(&trains[i], &now_sched, today);
            
            if(trains[i].status.is_active){

                ++active_trains;

                uint8_t row, col;

                // convert from checkpoint and percentage to rows and columns of the LED matrix
                train_to_led(&trains[i], &row, &col);

                led_matrix |= ((uint64_t)1 << col) << (8 * row);
            }
        }

        xQueueOverwrite(matrix_queue, &led_matrix);

        if(!active_trains){

            uint8_t id_last, id_first;

            schedule_t next_train_time = {0};
            // init at 15 in case I'm here but I have not passed the last train's time,
            // I'm going to wait for just 15s
            uint16_t sleep_time_s = 15;

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
                next_train_time = first_train[id_first];

            if(now_sched.hour > 23)
                now_sched.hour -= 24; // Needed
            sleep_time_s = schedule_dist(&now_sched, &next_train_time);

            ESP_LOGI("main", "sleep time is: %us", sleep_time_s);

            if(sleep_time_s > 60)
                esp_deep_sleep(5 * 1e6);//sleep_time_s * 1000000);
            
            if(sleep_time_s > 14)
                vTaskDelay(MSEC(sleep_time_s * 1000));
        }

        vTaskDelay(MSEC(2000));
    }
}
