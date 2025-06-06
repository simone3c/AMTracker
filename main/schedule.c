#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "schedule.h"

int schedule_cmp(const schedule_t* s1, const schedule_t* s2){

    int ret = memcmp(s1, s2, sizeof(*s1));

    return ret > 0 ? 1 : ret < 0 ? -1 : 0;
}

uint32_t schedule_diff(const schedule_t* s1, const schedule_t* s2){

    uint32_t s1_sec = (s1->hour * 60 + s1->min) * 60 + s1->sec;
    uint32_t s2_sec = (s2->hour * 60 + s2->min) * 60 + s2->sec;

    // if s1 == 10:00 and s2 == 9:00 I want the result to be 23 hours, so it's
    // easier to trasform s2 into 33:00 by adding one day (24*60*60 seconds)
    while(s2_sec < s1_sec)
        s2_sec += 24 * 60 * 60;
    
    return s2_sec - s1_sec;
}

float get_percentage(const schedule_t* start, const schedule_t* end, const schedule_t* time){

    // ensures that the percentage is meaningful + avoids division by 0 (c1 == 0)
    assert(schedule_cmp(start, end) == -1);

    uint32_t c1 = schedule_diff(start, end);
    uint32_t c2 = schedule_diff(start, time);

    //ESP_LOGI("get_pecentage", "c1: %"PRIi32" - c2: %"PRIi32" - p: %f", c1, c2, (float)c2 / c1);

    return (float)c2 / c1;
}

bool is_between(const schedule_t* start, const schedule_t* end, const schedule_t* time){

    float p = get_percentage(start, end, time);

    return !(p < 0. || p > 1.) ;
}