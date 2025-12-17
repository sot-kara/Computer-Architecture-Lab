#include <stdint.h>
#include <stdio.h>
#define WIDTH 3
#define HEIGHT 3
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


    /* Default I/O implies 32 bit AXI4-DATA_WIDTH. Therefore we're starting with one integer read per cycle. */

    void IMAGE_DIFF_POSTERIZE(const uint8_t *in_A, const uint8_t  *in_B, uint8_t *out, unsigned int size){

        #pragma HLS INTERFACE m_axi port = in_A offset = slave bundle = gmem
        #pragma HLS INTERFACE m_axi port = in_B offset = slave bundle = gmem
        #pragma HLS INTERFACE m_axi port = out offset = slave bundle = gmem
        #pragma HLS INTERFACE s_axilite port = in_A bundle = control
        #pragma HLS INTERFACE s_axilite port = in_B bundle = control
        #pragma HLS INTERFACE s_axilite port = out bundle = control
        #pragma HLS INTERFACE s_axilite port = size bundle = control
        #pragma HLS INTERFACE s_axilite port = return bundle = control
                

        unsigned int v1_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector1
        unsigned int v2_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector2
        unsigned int vout_buffer[BUFFER_HEIGHT][BUFFER_WIDTH]; // Local Memory to store result
        int temp_filter;

        // Per iteration of this loop perform BUFFER_SIZE vector addition
        Chunk_loop: for (int i = 0; i < size; i += 1) {
        #pragma HLS LOOP_TRIPCOUNT min = c_len max = c_len

        // boundary checks
            if(i/WIDTH + BUFFER_HEIGHT > HEIGHT )
                break;
            if ((i % WIDTH) + BUFFER_WIDTH > WIDTH) {
                i += WIDTH - (i % WIDTH)-1;
                continue;
            }
            
            


            read:    for (int j = 0; j < BUFFER_HEIGHT; j++) {
                for(int k = 0; k < BUFFER_WIDTH; k++){
            #pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
            #pragma HLS PIPELINE II = 1

                v1_buffer[j][k] = in_A[i + j*BUFFER_WIDTH + k];
                v2_buffer[j][k] = in_B[i + j*BUFFER_WIDTH + k];
                }
            }

            // PIPELINE pragma reduces the initiation interval for loop by allowing the
            // concurrent executions of operations
            process:     for (int j = 0; j < BUFFER_HEIGHT; j++) {
                for(int k = 0; k < BUFFER_WIDTH; k++){
            #pragma HLS LOOP_TRIPCOUNT min = c_size max = c_size
            #pragma HLS PIPELINE II = 1
                vout_buffer[j][k] = v1_buffer[j][k] + v2_buffer[j][k];
                vout_buffer[j][k] = Compare((uint8_t) v1_buffer[j][k], (uint8_t) v2_buffer[j][k]);
            }
        }

/*
            // print buffer for debugging
            printf("Buffer is: \n");
            write:    for (int j = 0; j < BUFFER_HEIGHT; j++) {
                for(int k = 0; k < BUFFER_WIDTH; k++){
                    printf("%d ",vout_buffer[j][k]);
        }
                    printf("\n");
            } */
        
    // Now we check for boundary pixels and copy them directly
    if(i/WIDTH ==0){
        for(int col=0; col<BUFFER_WIDTH; col++){
            // If first row, write first row of buffer
                out[i + col] = vout_buffer[0][col];
        }
    }
    
    if(i/WIDTH == HEIGHT - BUFFER_HEIGHT){
        for(int col=0; col<BUFFER_WIDTH; col++){
            // If last row, write last row of buffer
            out[i + (2*WIDTH) + col] = vout_buffer[2][col];
        }
    }
    if(i%WIDTH == 0){
        for(int row=0; row<BUFFER_HEIGHT; row++){
            // If first column, write first column of buffer
            out[i + row*WIDTH] = vout_buffer[row][0];
        }
    }

    if(i%WIDTH == WIDTH - BUFFER_WIDTH){
        for(int row=0; row<BUFFER_HEIGHT; row++){
            // If last column, write last column of buffer
            out[i + row*WIDTH + 2] = vout_buffer[row][2];
        }

    }
    // For non-boundary pixels, apply filter and write result to global memory clipping to [0,255]
        temp_filter = 5*vout_buffer[1][1] - vout_buffer[1][2] - vout_buffer[1][0] - vout_buffer[2][1] - vout_buffer[0][1]; 
        out[i + WIDTH + 1] = (uint8_t)(temp_filter<0 ? 0 : (temp_filter>255 ? 255 : temp_filter) );
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




int main(){

    const int size = 9;
    uint8_t A[size], B[size];
    uint8_t out[size];
    
    
    for(int i=0; i<size; i++){
        A[i] = 100;
        B[i] = 1;
        out[i] = 0;
    }
    
    IMAGE_DIFF_POSTERIZE(A,B,out,size);
    
    for(int i =0 ; i<size; i++){
        printf("%d ", out[i]);
    }
}
