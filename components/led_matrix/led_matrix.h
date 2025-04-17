#ifndef MY_LED_MATRIX
#define MY_LED_MATRIX

#include <inttypes.h>

// row and cols are bitmaps that select which row and columns to light up.
// row contains "1" for the row to be lit up
// cols contains "0" for the columns to be lit up (due to common anode)
void matrix_draw(uint8_t row, uint8_t cols);

#endif