#ifndef MY_sn74hc595
#define MY_sn74hc595

#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>

void sn74hc595_set_data_pin(bool v);
void sn74hc595_set_clk_pin(bool v);
void sn74hc595_set_latch_pin(bool v);

void sn74hc595_write(const uint8_t* data, size_t sz, bool msb_first);
void sn74hc595_write_byte(uint8_t byte, bool msb_first);


#endif