#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <hls_stream.h>
#include <ap_int.h>

// CRITICAL: Must match Kernel Dimensions
#define WIDTH 128
#define HEIGHT 128

typedef ap_uint<512> wide_t;

void IMAGE_DIFF_POSTERIZE(hls::stream<wide_t> &stream_A, 
                          hls::stream<wide_t> &stream_B, 
                          hls::stream<wide_t> &stream_C);

// -----------------------------------------------------------
// GLOBAL ALLOCATION (Safe from Stack Overflow)
// -----------------------------------------------------------
// Allocating 256x256 arrays on the stack (inside main) often causes 
// C Simulation to crash silently on Windows. We move them here.
uint8_t A[HEIGHT][WIDTH];
uint8_t B[HEIGHT][WIDTH];
uint8_t C[HEIGHT][WIDTH];
uint8_t ref_data[HEIGHT][WIDTH];

int main() {
    printf("Initializing 256x256 data...\n");
    for(int i = 0; i < HEIGHT; i++) {
        for(int j = 0; j < WIDTH; j++) {
            A[i][j] = (uint8_t)((i * j) % 256);
            B[i][j] = (uint8_t)((i / (j + 1)) % 256);
        }
    }

    hls::stream<wide_t> strm_A;
    hls::stream<wide_t> strm_B;
    hls::stream<wide_t> strm_C;

    int chunks_per_row = WIDTH / 64;

    printf("Packing streams...\n");
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < chunks_per_row; j++) {
            wide_t pack_a = 0;
            wide_t pack_b = 0;

            for (int k = 0; k < 64; k++) {
                int col_idx = (j * 64) + k;
                int start = k * 8;
                int end = start + 7;
                
                pack_a.range(end, start) = A[i][col_idx];
                pack_b.range(end, start) = B[i][col_idx];
            }
            strm_A.write(pack_a);
            strm_B.write(pack_b);
        }
    }

    printf("Running Kernel...\n");
    IMAGE_DIFF_POSTERIZE(strm_A, strm_B, strm_C);

    printf("Unpacking results...\n");
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < chunks_per_row; j++) {
            if (strm_C.empty()) {
                printf("Error: Stream C is empty prematurely at Row %d Col Chunk %d!\n", i, j);
                printf("Likely mismatch between TB Dimensions and Kernel Dimensions.\n");
                return 1;
            }
            wide_t pack_c = strm_C.read();

            for (int k = 0; k < 64; k++) {
                int col_idx = (j * 64) + k;
                int start = k * 8;
                int end = start + 7;
                C[i][col_idx] = (uint8_t)pack_c.range(end, start);
            }
        }
    }

    printf("Verifying...\n");
    // Simple self-verification logic (re-implementing math)
    // This avoids File I/O issues for quick debugging
    bool match = true;
    for (int i = 0; i < HEIGHT; i++) {
        for(int j = 0; j < WIDTH; j++) {
            int16_t temp_d = (int16_t)A[i][j] - (int16_t)B[i][j];
            uint8_t D = (temp_d < 0) ? -temp_d : temp_d;
            uint8_t expected;
            
            if(D < 32) expected = 0;
            else if(D < 96) expected = 128;
            else expected = 255;

            if (C[i][j] != expected) {
                printf("Mismatch at (%d,%d): HW=%d vs Expected=%d\n", i, j, C[i][j], expected);
                match = false;
                return 1;
            }
        }
    }

    if(match) printf("Test PASSED!\n");
    else printf("Test FAILED!\n");
    
    return 0;
}