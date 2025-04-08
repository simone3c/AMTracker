#ifndef MY_TRAIN
#define MY_TRAIN

#include "schedule.h"

#define MAX_LINE_STOPS (10)

typedef enum{
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
    DEFERRARI_BIRGNOLE
} checkpoint_t;

typedef struct{
    char name[30];
    checkpoint_t path[2 * MAX_LINE_STOPS - 1];
    size_t stops_num;
} line_t;

typedef struct{
    int id;
    schedule_t arrival[MAX_LINE_STOPS];
    schedule_t departure[MAX_LINE_STOPS];
    day_t day;
    line_t* line;
} train_t;

typedef struct{
    checkpoint_t pos;
    float perc;
} train_position_t;

void print_train(const train_t* t);

#endif