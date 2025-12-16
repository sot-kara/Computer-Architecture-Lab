#include <stdint.h>

#define WIDTH 256
#define HEIGHT 256
#define BUFFER_HEIGHT 1
#define BUFFER_WIDTH 256
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

    void IMAGE_DIFF_POSTERIZE(const uint8_t *in_A, const uint8_t  *in_B, uint8_t *out, unsigned int size){

        // TODO : Change testbench
                
        #pragma HLS INTERFACE m_axi port = in_A offset = slave bundle = gmem
        #pragma HLS INTERFACE m_axi port = in_B offset = slave bundle = gmem
        #pragma HLS INTERFACE m_axi port = out offset = slave bundle = gmem
        #pragma HLS INTERFACE s_axilite port = in_A bundle = control
        #pragma HLS INTERFACE s_axilite port = in_B bundle = control
        #pragma HLS INTERFACE s_axilite port = out bundle = control
        #pragma HLS INTERFACE s_axilite port = size bundle = control
        #pragma HLS INTERFACE s_axilite port = return bundle = control

        unsigned int v1_buffer[BUFFER_SIZE];   // Local memory to store vector1
        unsigned int v2_buffer[BUFFER_SIZE];   // Local memory to store vector2
        unsigned int vout_buffer[BUFFER_SIZE]; // Local Memory to store result

        // Per iteration of this loop perform BUFFER_SIZE vector addition
        Chunk_loop: for (int i = 0; i < size; i += BUFFER_SIZE) {
        #pragma HLS LOOP_TRIPCOUNT min = c_len max = c_len

            int chunk_size = BUFFER_SIZE;
            // boundary checks
            if ((i + BUFFER_SIZE) > size)
            chunk_size = size - i;


            read1:    for (int j = 0; j < chunk_size; j++) {
            #pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
            #pragma HLS PIPELINE II = 1
                v1_buffer[j] = in_A[i + j];
            }

            read2:    for (int j = 0; j < chunk_size; j++) {
            #pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
            #pragma HLS PIPELINE II = 1
                v2_buffer[j] = in_B[i + j];
            }

            // PIPELINE pragma reduces the initiation interval for loop by allowing the
            // concurrent executions of operations
            process:     for (int j = 0; j < chunk_size; j++) {
            #pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
            #pragma HLS PIPELINE II = 1
                // perform vector addition
                vout_buffer[j] = v1_buffer[j] + v2_buffer[j];
                vout_buffer[j] = Compare((uint8_t) v1_buffer[j], (uint8_t) v2_buffer[j]);
            }

            // burst write the result
            write:    for (int j = 0; j < chunk_size; j++) {
            #pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
            #pragma HLS PIPELINE II = 1
                out[i + j] = vout_buffer[j];
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