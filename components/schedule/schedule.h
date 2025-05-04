#ifndef MY_SCHEDULE
#define MY_SCHEDULE

#include <stdbool.h>

typedef struct{
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} schedule_t;

// returns 1 if s1 > s2, 0 if they are equal, -1 if s1 < s2
int schedule_cmp(const schedule_t* s1, const schedule_t* s2);

// distance, in seconds, between s1 and s2.
// requires s1 and s2 in H24 format
// get_daley(10:00:0:, 12:00:00) -> 720
// get_daley(10:00:0:, 9:00:00) -> 82800
uint32_t schedule_dist(const schedule_t* s1, const schedule_t* s2);

// requires that start comes before end
float get_percentage(const schedule_t* start, const schedule_t* end, const schedule_t* time);

// requires that start comes before end
bool is_between(const schedule_t* start, const schedule_t* end, const schedule_t* time);

#endif