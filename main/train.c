#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "train.h"

static const char* checkpoint_str_map[] = {
    [BRIN] = "brin",
    [DINEGRO] = "dinegro",
    [PRINCIPE] = "principe",
    [DARSENA] = "darsena",
    [SANGIORGIO] = "sangiorgio",
    [SARZANO] = "sarzano",
    [DEFERRARI] = "deferrari",
    [BRIGNOLE] = "brignole",
    [BRIN_DINEGRO] = "brin_dinegro",
    [DINEGRO_PRINCIPE] = "dinegro_principe",
    [PRINCIPE_DARSENA] = "principe_darsena",
    [DARSENA_SANGIORGIO] = "darsena_sangiorgio",
    [SANGIORGIO_SARZANO] = "sangiorgio_sarzano",
    [SARZANO_DEFERRARI] = "sarzano_deferrari",
    [DEFERRARI_BIRGNOLE] = "deferrari_brignole"
};

const char* checkpoint_str(checkpoint_t c){
    return checkpoint_str_map[c];
}

bool is_station(checkpoint_t c){
    return c == BRIN || c == DINEGRO || c == PRINCIPE || c == DARSENA ||
    c == SANGIORGIO || c == SARZANO || c == DEFERRARI || c == BRIGNOLE;
}

bool line_eq(const line_t* l1, const line_t* l2){
    return !strcmp(l1->name, l2->name);
}

void update_train_position(train_t* train, const schedule_t* time){
    // check if train is stopped in a station
    for(int j = 0; j < train->line->stops_num; ++j){
        if(is_between(&train->arrival[j], &train->departure[j], time)){

            train->status.position.checkpoint = train->line->path[j * 2]; // 2* selects only stations
            train->status.position.perc = 0.0; // doesnt care since it's in a station
            return;
        }
    }

    // check if train is moving between 2 stations
    for(int j = 0; j < train->line->stops_num - 1; ++j){
        if(is_between(&train->departure[j], &train->arrival[j + 1], time)){

            train->status.position.checkpoint = train->line->path[j * 2 + 1]; // +1 selects only intermediate points between statons
            train->status.position.perc = get_percentage(&train->departure[j], &train->arrival[j + 1], time);
            return;
        }
    }

    assert(false);
}

void update_train_status(train_t* train, const schedule_t* time, day_t day){

    if(day != SUNDAY && day != SATURDAY)
        day = MON_FRI;

    train->status.is_active =
        train->day == day &&
        is_between(&train->arrival[0], &train->departure[train->line->stops_num - 1], time);

    if(train->status.is_active)
        update_train_position(train, time);
}