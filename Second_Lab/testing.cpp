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
                

        unsigned int v1_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector1
        unsigned int v2_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector2
        unsigned int vout_buffer[BUFFER_HEIGHT][BUFFER_WIDTH]; // Local Memory to store result
        int temp_filter;

        // Per iteration of this loop perform BUFFER_SIZE vector addition
        Chunk_loop: for (int i = 0; i < size; i += 1) {
// Handle boundary pixels
if (i / WIDTH == 0 || i / WIDTH == HEIGHT - 1 || 
    i % WIDTH == 0 || i % WIDTH == WIDTH - 1) {
        
        out[i] = 0; // Center pixel
}
else{
read: for (int j = 0; j < BUFFER_HEIGHT; j++) {
    for (int k = 0; k < BUFFER_WIDTH; k++) {

        // Calculate the global row and column indices for the current window pixel
        int row = i / WIDTH + j - 1;  // Offset by -1 for the 3x3 relative to center
        int col = i % WIDTH + k - 1;

        // Boundary check: Use zero-padding when out of bounds
        bool valid = row >= 0 && row < HEIGHT && col >= 0 && col < WIDTH;

        // Safely populate the buffer
        v1_buffer[j][k] = valid ? in_A[row * WIDTH + col] : 0;
        v2_buffer[j][k] = valid ? in_B[row * WIDTH + col] : 0;
    }
}
printf("Buffers after reading input:\n");
for(int j=0; j < BUFFER_HEIGHT; j++) {
    for(int k=0; k < BUFFER_WIDTH; k++){
        vout_buffer[j][k] = Compare((uint8_t)v1_buffer[j][k], (uint8_t)v2_buffer[j][k]);
        printf("%d ", vout_buffer[j][k]);
    }
    printf("\n");
}

// Perform filter processing
process:

        // Compute filtered value for the center
        temp_filter = 5 * vout_buffer[1][1] 
                      - vout_buffer[0][1] // Top
                      - vout_buffer[2][1] // Bottom
                      - vout_buffer[1][0] // Left
                      - vout_buffer[1][2]; // Right

        // Clip result and write to "out" buffer
        out[i] = 
            (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));



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




int main(){

    const int size = WIDTH * HEIGHT;
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
