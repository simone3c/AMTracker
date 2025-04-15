#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "train.h"

bool is_station(checkpoint_t c){
    return c == BRIN || c == DINEGRO || c == PRINCIPE || c == DARSENA || 
    c == SANGIORGIO || c == SARZANO || c == DEFERRARI || c == BRIGNOLE;
}

bool line_cmp(const line_t* l1, const line_t* l2){
    return strlen(l1->name) == strlen(l2->name) && !strcmp(l1->name, l2->name);
}

void print_train(const train_t* t){
    // ESP_LOGI("print_train", "id: %i - day: %i - line: %s", t->id, (int)t->day, t->line->name);
    // for(int i = 0; i < 8; ++i){
    //     ESP_LOGI("print_train", "stop[%i]: %"PRIu8":%"PRIu8":%"PRIu8" - %"PRIu8":%"PRIu8":%"PRIu8"",
    //         i,
    //         t->arrival[i].hour,
    //         t->arrival[i].min,
    //         t->arrival[i].sec,
    //         t->departure[i].hour,
    //         t->departure[i].min,
    //         t->departure[i].sec
    //     );
    // }
}
