#ifndef MY_SCHEDULE
#define MY_SCHEDULE

#include <stdbool.h>

typedef enum{
    MON_FRI,
    SATURDAY,
    SUNDAY
} day_t;

typedef struct{
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} schedule_t;

void print_schedule(const schedule_t* s);
// 1 if s1 > s2 - 0 if equal - -1 if <
int schedule_cmp(const schedule_t* s1, const schedule_t* s2);
// delay, in seconds, between s1 and s2.
// negative if s2 comes bfore s1
int32_t get_delay(const schedule_t* s1, const schedule_t* s2);

// assumes that start is before end
// assumes "time" between 00:00 and 23:59:59
float get_percentage(const schedule_t* start, const schedule_t* end, schedule_t time);

// assumes that start is before end
bool is_between(const schedule_t* start, const schedule_t* end, schedule_t time);

#endif