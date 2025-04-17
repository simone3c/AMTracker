#include <stdio.h>
#include "led_matrix.h"
#include "sn74hc595.h"
#include <stdbool.h>

// row and cols are bitmaps that select which row and columns to light up.
// row contains "1" for the row to be lit up
// cols contains "0" for the columns to be lit up (due to common anode)
void matrix_draw(uint8_t row, uint8_t cols){
    // bit-wise not is needed because the matrix is in common anode
    uint8_t data[] = {~cols, row};
    sn74hc595_write(data, 2, false);
}
