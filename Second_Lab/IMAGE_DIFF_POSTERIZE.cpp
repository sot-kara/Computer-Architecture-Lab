#include <stdint.h>

#define WIDTH 256
#define HEIGHT 256
#define BUFFER_HEIGHT 3
#define BUFFER_WIDTH 3
#define T1 32
#define T2 96


const int BUFFER_SIZE = BUFFER_HEIGHT*BUFFER_WIDTH;

//  For Lab 3:
//#include "ap_int.h"           // use this type for function i/o and handle as packages
//typedef ap_uint<512> uint512_dt;

// TRIPCOUNT identifier
const unsigned int c_size = BUFFER_SIZE;
const unsigned int c_len = HEIGHT*WIDTH / c_size;

uint8_t Compare(uint8_t A, uint8_t B);


extern "C" {
    /* Default I/O implies 32 bit AXI4-DATA_WIDTH. Therefore we're starting with one integer read per cycle. */

void IMAGE_DIFF_POSTERIZE(const uint8_t *in_A, const uint8_t *in_B, uint8_t *out, unsigned int size)
{

#pragma HLS INTERFACE m_axi port = in_A offset = slave bundle = gmem0
#pragma HLS INTERFACE m_axi port = in_B offset = slave bundle = gmem1
#pragma HLS INTERFACE m_axi port = out offset = slave bundle = gmem2
#pragma HLS INTERFACE s_axilite port = in_A bundle = control
#pragma HLS INTERFACE s_axilite port = in_B bundle = control
#pragma HLS INTERFACE s_axilite port = out bundle = control
#pragma HLS INTERFACE s_axilite port = size bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

    uint8_t v1_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector1
    uint8_t v2_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector2
    uint8_t vout_buffer[BUFFER_HEIGHT][BUFFER_WIDTH]; // Local Memory to store result
    int temp_filter;

#pragma HLS ARRAY_PARTITION variable = v1_buffer dim = 0 complete
#pragma HLS ARRAY_PARTITION variable = v2_buffer dim = 0 complete
#pragma HLS ARRAY_PARTITION variable = vout_buffer dim = 0 complete
// Per iteration of this loop perform BUFFER_SIZE vector addition
Chunk_loop:
    for (int i = 0; i < size; i += 1)
    {
#pragma HLS LOOP_TRIPCOUNT min = c_len max = c_len
#pragma HLS PIPELINE II = 1
        // Handle boundary pixels
        if (i / WIDTH == 0 || i / WIDTH == HEIGHT - 1 ||
            i % WIDTH == 0 || i % WIDTH == WIDTH - 1)
        {

            out[i] = 0; // Center pixel
        }
        else
        {
        read:
            for (int j = 0; j < BUFFER_HEIGHT; j++)
            {
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {
#pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
#pragma HLS PIPELINE II = 1

                    // Calculate the global row and column indices for the current window pixel
                    int row = i / WIDTH + j - 1; // Offset by -1 for the 3x3 relative to center
                    int col = i % WIDTH + k - 1;

                    // Safely populate the buffer
                    v1_buffer[j][k] = in_A[row * WIDTH + col];
                    v2_buffer[j][k] = in_B[row * WIDTH + col];
                }
            }
            for (int j = 0; j < BUFFER_HEIGHT; j++)
            {
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {
				#pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
				#pragma HLS UNROLL
                    vout_buffer[j][k] = Compare((uint8_t)v1_buffer[j][k], (uint8_t)v2_buffer[j][k]);
                }
            }

            // Compute filtered value for the center
            temp_filter = 5 * vout_buffer[1][1] - vout_buffer[0][1] // Top
                          - vout_buffer[2][1]                       // Bottom
                          - vout_buffer[1][0]                       // Left
                          - vout_buffer[1][2];                      // Right

            // Clip result and write to "out" buffer
            out[i] =
                (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));
        }
    }
}

}

uint8_t Compare(uint8_t A, uint8_t B){
    uint8_t C;
    int16_t temp_d = (int16_t) A - (int16_t) B;

    // Note: Since temp_d is unsigned (uint8_t), it can never be < 0.
    // This logic performs a modular subtraction check.
    uint8_t D = (temp_d < 0) ? -temp_d : temp_d;

    if(D < T1) C = 0;
    else if(D < T2) C = 128;
    else C = 255;

    return C;
}
