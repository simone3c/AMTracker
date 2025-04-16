#include "sn74hc595.h"

void sn74hc595_write(const uint8_t* data, size_t sz, bool msb_first){

    sn74hc595_set_latch_pin(0);
    sn74hc595_set_clk_pin(0);

    for(int b = 0; b < sz; ++b){
        
        for(int i = 0; i < 8; ++i){
            if(msb_first)
                sn74hc595_set_data_pin(data[b] & (1 << (7 - i)));
            else
                sn74hc595_set_data_pin(data[b] & (1 << i));
            
            sn74hc595_set_clk_pin(1);
            sn74hc595_set_clk_pin(0);
        }
    }
    sn74hc595_set_latch_pin(1);
    // maybe add some delay
    sn74hc595_set_latch_pin(0);
}

void sn74hc595_write_byte(uint8_t byte, bool msb_first){
    sn74hc595_write(&byte, 1, msb_first);
}



