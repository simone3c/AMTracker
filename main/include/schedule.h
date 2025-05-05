#ifndef MY_SCHEDULE
#define MY_SCHEDULE

// ! GTFS's support for >24 hour format (i.e. 01:00 == 25:00) and so does this project

#include <stdbool.h>

typedef struct{
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} schedule_t;

// returns 1 if s1 > s2, 0 if they are equal, -1 if s1 < s2
int schedule_cmp(const schedule_t* s1, const schedule_t* s2);

// difference, in seconds, between s1 and s2.
// assumes s1 comes before s2
// get_daley(10:00:0, 12:00:00) -> 720
// get_daley(10:00:0, 9:00:00) -> 82800
// get_daley(10:00:0, 33:00:00) -> 82800
uint32_t schedule_diff(const schedule_t* s1, const schedule_t* s2);

// requires that start comes before end
float get_percentage(const schedule_t* start, const schedule_t* end, const schedule_t* time);

// requires that start comes before end
bool is_between(const schedule_t* start, const schedule_t* end, const schedule_t* time);

#endif