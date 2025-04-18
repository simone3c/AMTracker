#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "schedule.h"


void print_schedule(const schedule_t* s){
    //ESP_LOGI("print_schedule", "%"PRIu8" - %"PRIu8" - %"PRIu8, s->hour, s->min, s->sec);
}
// 1 if s1 > s2 - 0 if equal - -1 if <
int schedule_cmp(const schedule_t* s1, const schedule_t* s2){

    int ret = memcmp(s1, s2, sizeof(*s1));

    return ret > 0 ? 1 : ret < 0 ? -1 : 0;
}
// delay, in seconds, between s1 and s2.
// negative if s2 comes bfore s1
int32_t get_delay(const schedule_t* s1, const schedule_t* s2){
    int32_t s1_sec = (s1->hour * 60 + s1->min) * 60 + s1->sec;
    int32_t s2_sec = (s2->hour * 60 + s2->min) * 60 + s2->sec;

    return s2_sec - s1_sec;
}

// assumes that start is before end
// assumes "time" between 00:00 and 23:59:59
float get_percentage(const schedule_t* start, const schedule_t* end, schedule_t time){
    int32_t c1 = get_delay(start, end);
    int32_t c2 = 0;

    assert(c1 > 0);

    // train starts (&& arrives) after midnight
    if(start->hour > 23)
        time.hour += 24;

    // train only arrives after midnight
    else if(end->hour > 23){
        // it's already midnight
        if(schedule_cmp(start, &time) != -1)
            time.hour += 24;
    }

    c2 = get_delay(start, &time);

    //ESP_LOGI("get_pecentage", "c1: %"PRIi32" - c2: %"PRIi32" - p: %f", c1, c2, (float)c2 / c1);

    return (float)c2 / c1;
}

// assumes that start is before end
bool is_between(const schedule_t* start, const schedule_t* end, schedule_t time){

    float p = get_percentage(start, end, time);

    return !(p < 0. || p > 1.) ;
}