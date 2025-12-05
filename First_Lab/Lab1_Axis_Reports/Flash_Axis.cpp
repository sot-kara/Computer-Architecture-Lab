#include <stdint.h>
#include <hls_stream.h>
#include <ap_int.h>

#define WIDTH 256
#define HEIGHT 256
#define T1 32
#define T2 96

// Define a 512-bit wide word (64 pixels * 8 bits)
typedef ap_uint<512> wide_t;

// Number of 512-bit chunks in a single row
#define CHUNKS_PER_ROW (WIDTH / 64) 

void IMAGE_DIFF_POSTERIZE(hls::stream<wide_t> &stream_A, hls::stream<wide_t> &stream_B, hls::stream<wide_t> &stream_C) {
    
    #pragma HLS INTERFACE axis port=stream_A
    #pragma HLS INTERFACE axis port=stream_B
    #pragma HLS INTERFACE axis port=stream_C

    uint8_t a; 
    uint8_t b; 
    uint8_t c;        // They are replacing the buffers

    ROW_LOOP: for(int i = 0; i < HEIGHT; i++) {
    #pragma HLS PIPELINE II=1

        COLUMN_LOOP: for(int j = 0; j < CHUNKS_PER_ROW; j++) {
        #pragma HLS PIPELINE II=1
            
            wide_t chunk_a = stream_A.read();
            wide_t chunk_b = stream_B.read();
            wide_t chunk_c;

            AXIS_PACKET_LOOP: for(int k = 0; k < 64; k++) {
            #pragma HLS UNROLL
                
                // axis loop pointers
                int pixel_idx = (j * 64) + k;
                int start_bit = k * 8;
                int end_bit   = start_bit + 7;

                // Input Caching
                a = chunk_a.range(end_bit, start_bit);
                b = chunk_b.range(end_bit, start_bit);

                // Processing
                int16_t temp_d = (int16_t)a - (int16_t)b;
                uint8_t D = (temp_d < 0) ? -temp_d : temp_d;
                if(D < T1)      c = 0;
                else if(D < T2) c = 128;
                else            c = 255;

                // Output Caching
                chunk_c.range(end_bit, start_bit) = c;
            }
            stream_C.write(chunk_c);
        }
    }
}