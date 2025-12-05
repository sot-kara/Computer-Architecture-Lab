#include <stdint.h>
#include <hls_stream.h>
#include <ap_int.h>


// Updated Dimensions
#define WIDTH 64
#define HEIGHT 64
#define T1 32
#define T2 96
#define PARTITION 64
// Number of 512-bit chunks in a single row
#define CHUNKS_PER_ROW (WIDTH / 64)  

// Define a 512-bit wide word (64 pixels * 8 bits)
typedef ap_uint<512> wide_t;


void IMAGE_DIFF_POSTERIZE(hls::stream<wide_t> &stream_A, hls::stream<wide_t> &stream_B, hls::stream<wide_t> &stream_C) {
    
    #pragma HLS INTERFACE axis port=stream_A
    #pragma HLS INTERFACE axis port=stream_B
    #pragma HLS INTERFACE axis port=stream_C


    // ----------------------------------------------
    // INTERNAL CACHE (Use static for the testbench)
    // ----------------------------------------------
    static uint8_t buf_A[HEIGHT][WIDTH];
    static uint8_t buf_B[HEIGHT][WIDTH];
    static uint8_t buf_C[HEIGHT][WIDTH];

    // Partition logic remains the same (explodes to registers)
    #pragma HLS ARRAY_PARTITION variable=buf_A type=cyclic dim=2 factor = PARTITION 
    #pragma HLS ARRAY_PARTITION variable=buf_B type=cyclic dim=2 factor = PARTITION
    #pragma HLS ARRAY_PARTITION variable=buf_C type=cyclic dim=2 factor = PARTITION


    LOAD_LOOP : for(int i = 0; i < HEIGHT; i++) {
    #pragma HLS PIPELINE II=1

        load_col_loop: for(int j = 0; j < CHUNKS_PER_ROW; j++) {
        #pragma HLS PIPELINE II=1
            
            wide_t chunk_a = stream_A.read();
            wide_t chunk_b = stream_B.read();

            unpack_loop: for(int k = 0; k < 64; k++) {
            #pragma HLS UNROLL
                
                int pixel_idx = (j * 64) + k;
                int start_bit = k * 8;
                int end_bit   = start_bit + 7;

                buf_A[i][pixel_idx] = chunk_a.range(end_bit, start_bit);
                buf_B[i][pixel_idx] = chunk_b.range(end_bit, start_bit);
            }
        }
    }


    PROCESSING_LOOP : for(int i = 0; i < HEIGHT; i++) {
    #pragma HLS PIPELINE II=1

        compute_col_loop: for(int j = 0 ; j < WIDTH ; j++){
        #pragma HLS UNROLL 

            int16_t temp_d = (int16_t)buf_A[i][j] - (int16_t)buf_B[i][j];
            uint8_t D = (temp_d < 0) ? -temp_d : temp_d;
            
            if(D < T1)      buf_C[i][j] = 0;
            else if(D < T2) buf_C[i][j] = 128;
            else            buf_C[i][j] = 255;
        }
    }


    STORE_LOOP: for(int i = 0; i < HEIGHT; i++) {
    #pragma HLS PIPELINE II=1
        store_col_loop: for(int j = 0; j < CHUNKS_PER_ROW; j++) {
        #pragma HLS PIPELINE II=1
            
            wide_t chunk_c;

            pack_loop: for(int k = 0; k < 64; k++) {
            #pragma HLS UNROLL
                
                int pixel_idx = (j * 64) + k;
                int start_bit = k * 8;
                int end_bit   = start_bit + 7;
                
                chunk_c.range(end_bit, start_bit) = buf_C[i][pixel_idx];
            }            
            stream_C.write(chunk_c);
        }
    }
}