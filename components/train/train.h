#ifndef MY_TRAIN
#define MY_TRAIN

#include "schedule.h"

#define MAX_LINE_STOPS (10)

typedef enum{
    SUNDAY,
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY,
    MON_FRI,
} day_t;

typedef enum{
    BRIN = 0,
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
    DEFERRARI_BIRGNOLE
} checkpoint_t;

typedef struct{
    char name[30];
    checkpoint_t path[2 * MAX_LINE_STOPS - 1];
    size_t stops_num;
} line_t;

typedef struct{
    checkpoint_t checkpoint;
    float perc;
} train_position_t;

typedef struct{
    bool is_active;
    train_position_t position;
} train_status_t;

typedef struct{
    int id;
    schedule_t arrival[MAX_LINE_STOPS];
    schedule_t departure[MAX_LINE_STOPS];
    day_t day;
    line_t* line;
    train_status_t status;
} train_t;

const char* checkpoint_str(checkpoint_t c);
bool is_station(checkpoint_t c);
bool line_eq(const line_t* l1, const line_t* l2);
void update_train_status(train_t* train, const schedule_t* time, day_t day);

#endif