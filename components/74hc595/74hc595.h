#ifndef MY_74hc595
#define MY_74hc595

#include <stdbool.h>



void write_byte(uint8_t data, bool msb_first);
void set_pin(int pin, uint8_t data);

#endif