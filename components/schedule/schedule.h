#ifndef MY_SCHEDULE
#define MY_SCHEDULE

#include <stdbool.h>

typedef struct{
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} schedule_t;

// 1 if s1 > s2 - 0 if equal - -1 if <
int schedule_cmp(const schedule_t* s1, const schedule_t* s2);

// delay, in seconds, between s1 and s2.
uint32_t get_delay(const schedule_t* s1, const schedule_t* s2);

// assumes that start comes before end
// assumes "time" between 00:00 and 23:59:59
float get_percentage(const schedule_t* start, const schedule_t* end, schedule_t time);

// assumes that start comes before end
bool is_between(const schedule_t* start, const schedule_t* end, schedule_t time);

#endif