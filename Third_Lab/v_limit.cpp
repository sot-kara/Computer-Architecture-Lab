#include <stdint.h>
#include <ap_int.h>
#include <hls_stream.h>

// Original Image
#define WIDTH 64
#define HEIGHT 64

// Transaction Definition
#define PIXEL_SIZE 8
#if WIDTH <= 64
    #define AXI_WIDTH_BITS WIDTH*PIXEL_SIZE        // Data width of Memory Access in bits per cycle
#else
    #define AXI_WIDTH_BITS 512       // Data width of Memory Access in bits per cycle
#endif
#define AXI_WIDTH_BYTES (AXI_WIDTH_BITS / PIXEL_SIZE)

// Buffer - Cached Image
#define BUFFER_HEIGHT 3
#if WIDTH <= 64
    #define BUFFER_WIDTH_CHUNKS 1
#else
	#define BUFFER_WIDTH_CHUNKS 2
#endif
#define BUFFER_WIDTH_BYTES (BUFFER_WIDTH_CHUNKS * AXI_WIDTH_BYTES)
#define V_LIMIT 256

const unsigned int h_steps = (WIDTH - BUFFER_WIDTH_BYTES) / (AXI_WIDTH_BYTES) + 1;

// Type Definitions
typedef ap_uint<AXI_WIDTH_BITS> uint512_dt;
typedef ap_uint<PIXEL_SIZE> pixel_t;

// Helper Functions
pixel_t Compare(pixel_t A, pixel_t B);

const unsigned int BUFFER_SIZE = BUFFER_HEIGHT*BUFFER_WIDTH_BYTES;
const unsigned int c_size = BUFFER_WIDTH_BYTES;
const unsigned int c_len = HEIGHT*WIDTH / c_size;

// MAIN
extern "C" {
    void IMAGE_DIFF_POSTERIZE(const uint512_dt *in_A, const uint512_dt *in_B, uint512_dt *out, unsigned int size)
    {
        // INTERFACE DIRECTIVES
        #pragma HLS INTERFACE m_axi port = in_A offset = slave bundle = gmem0
        #pragma HLS INTERFACE m_axi port = in_B offset = slave bundle = gmem1
        #pragma HLS INTERFACE m_axi port = out offset = slave bundle = gmem2
        #pragma HLS INTERFACE s_axilite port = in_A bundle = control
        #pragma HLS INTERFACE s_axilite port = in_B bundle = control
        #pragma HLS INTERFACE s_axilite port = out bundle = control
        #pragma HLS INTERFACE s_axilite port = size bundle = control
        #pragma HLS INTERFACE s_axilite port = return bundle = control

        // Local Buffers
        pixel_t Prior_chunk_1[BUFFER_WIDTH_BYTES];
        pixel_t Prior_chunk_2[BUFFER_WIDTH_BYTES];
        pixel_t Filtered_chunk[BUFFER_WIDTH_BYTES];
        pixel_t inter_pixels[2][V_LIMIT];

        #pragma HLS ARRAY_PARTITION variable=inter_pixels complete dim=1
        #pragma HLS ARRAY_PARTITION variable=Prior_chunk_1 complete
        #pragma HLS ARRAY_PARTITION variable=Prior_chunk_2 complete
        #pragma HLS ARRAY_PARTITION variable=Filtered_chunk complete
        #pragma HLS ARRAY_PARTITION variable=inter_pixels complete

        // Stream Declaration
        hls::stream<uint512_dt> stream_G;  // We don't mind that it is HEIGHT sized, finally becomes just a stream of fixed depth.



        // Calculate number of vertical steps
        const int v_steps = (HEIGHT % V_LIMIT) ? (HEIGHT / V_LIMIT) + 1 : (HEIGHT / V_LIMIT);

        for (int v_step = 0; v_step < v_steps; v_step++){
            // Calculating first & last row of every v_step
            int ref_row = (v_step == 0) ? 0 : v_step * V_LIMIT - 2;                // - 2 to include the two previous rows for filtering

                // Last Row of step: Is the last step AND  Height not multiple of V_LIMIT then adjust else normal last = v_step*V_LIMIT -1
            unsigned int last_row = ((v_step == v_steps - 1) && (HEIGHT % V_LIMIT)) ? ref_row + (HEIGHT % V_LIMIT) - 1 : v_step * V_LIMIT - 1;
            if (v_step == 0) {
                last_row = V_LIMIT - 1;
            }

            // Special case for single step to include last row and start from 0
            if (v_steps == 1) {
                last_row = HEIGHT;
                ref_row = 0;
            }



            // Shift the buffer horizontally
            for (int h_step = 0; h_step < h_steps; h_step++) {

            // Stream to connect Stage 1 of Comparison and Stage 2 of filtering
            #pragma HLS DATAFLOW
            #pragma HLS STREAM variable=stream_G depth = 100     // TODO: We may need to adjust fifo space 

                int curr_buf = h_step % 2;
                int prev_buf = 1 - curr_buf;

                // Loop the buffer and compare over all image rows
                for (int row = ref_row; row < last_row; row++) {
                    unsigned int href_point = h_step * AXI_WIDTH_BYTES;
                    unsigned int ref_point = (row*WIDTH + href_point)/AXI_WIDTH_BYTES;


                    for (int i = 0; i < BUFFER_WIDTH_CHUNKS; i++) {
                    #pragma HLS PIPELINE II=1

                        uint512_dt val1 = in_A[ref_point + i];
                        uint512_dt val2 = in_B[ref_point + i];
                        uint512_dt res_G;

                        // Compute Compare (Posterize) on all AXI PIXELS (64 elements) in parallel
                        for (int v = 0; v < AXI_WIDTH_BYTES; v++) {
                            #pragma HLS UNROLL

                            // Unpack
                            pixel_t p1 = val1.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);
                            pixel_t p2 = val2.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);

                            // Compare
                            pixel_t g_val = Compare(p1, p2);

                            // Re-pack
                            res_G.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE) = g_val;
                        }

                        // Push G result to the stream for the next stage
                        stream_G.write(res_G);
                    }
                }

                for(int k=0; k<BUFFER_WIDTH_BYTES; k++) {
                    #pragma HLS UNROLL
                    Prior_chunk_1[k] = 0;
                    Prior_chunk_2[k] = 0;
                }

                // Filter the buffer over rows and output
                for (int row = ref_row; row < last_row; row++) {

                    // Declaring here for hinting a temporary storage
                    pixel_t Current_chunk[BUFFER_WIDTH_BYTES];
                    #pragma HLS ARRAY_PARTITION variable=Current_chunk complete

                    unsigned int href_point = h_step * AXI_WIDTH_BYTES;

                    // Row chunk unpacking
                    for (int chunk = 0; chunk < BUFFER_WIDTH_CHUNKS; chunk++) {
                    #pragma HLS PIPELINE II=1

                        uint512_dt temp_val = stream_G.read();

                        for (int v = 0; v < AXI_WIDTH_BYTES; v++) {
                        #pragma HLS UNROLL
                            Current_chunk[chunk*AXI_WIDTH_BYTES + v] = temp_val.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);
                        }
                    }

                    if (row >= ref_row + 2) {  // All lines available, normal operation

                        // Filtering Logic
                        if(href_point == 0){
                            // First pixel of the chunk at the left border
                            Filtered_chunk[0] = 0;
                        }
                        else{
                            if(WIDTH >64)
                            Filtered_chunk[0] = inter_pixels[prev_buf][row - 2 - ref_row];
                        }

                        for (int col = 1; col < BUFFER_WIDTH_BYTES; col++) {
                        #pragma HLS UNROLL

                            if (col == BUFFER_WIDTH_BYTES - 1){
                                Filtered_chunk[col] = 0;
                            }
                            else {
                                int16_t temp_filter = 5 * Prior_chunk_1[col]        // Center pixel
                                                        - Current_chunk[col]        // Down pixel
                                                        - Prior_chunk_2[col]        // Up pixel
                                                        - Prior_chunk_1[col - 1]    // Left pixel
                                                        - Prior_chunk_1[col + 1];   // Right pixel

                                // Clamping the result to [0, 255]
                                pixel_t filtered_pixel = (pixel_t) (temp_filter < 0) ? 0 : (temp_filter > 255 ? 255 : temp_filter);

                                // Write back to output buffer
                                Filtered_chunk[col] = filtered_pixel;
                            }
                        }

                        // Middle pixel of the chunk
                        if(WIDTH > 64)
                            inter_pixels[curr_buf][row - 2 - ref_row] = Filtered_chunk[BUFFER_WIDTH_BYTES/BUFFER_WIDTH_CHUNKS];

                        // Write the filtered chunk to output stream
                        uint512_dt out_val;
                        unsigned int write_row_offset = ((row - 1) * WIDTH + href_point) / AXI_WIDTH_BYTES;

                        for (int chunk = 0; chunk < BUFFER_WIDTH_CHUNKS; chunk++) {
                        #pragma HLS PIPELINE II=1
                            // Output packing
                            for (int v = 0; v < AXI_WIDTH_BYTES; v++) {
                            #pragma HLS UNROLL
                                out_val.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE) = Filtered_chunk[chunk*AXI_WIDTH_BYTES + v];
                            }

                            out[write_row_offset + chunk] = out_val;
                        }
                    }

                    // Refresh the Chunks
                    for (int v = 0; v < BUFFER_WIDTH_BYTES; v++) {
                    #pragma HLS UNROLL
                        Prior_chunk_2[v] = Prior_chunk_1[v];
                        Prior_chunk_1[v] = Current_chunk[v];
                    }
                }
            }
        }
        
        // Make the first and last row zeros
        for(int chunk=0; chunk < WIDTH/AXI_WIDTH_BYTES; chunk++){
            out[chunk] = 0;
            out[(HEIGHT-1)*(WIDTH/AXI_WIDTH_BYTES) + chunk] = 0;
        }
    }
}


/* Compare Helper Function
    - Input  : 2 uint8_t numbers
    - Output : Quantized absolute difference
*/
pixel_t Compare(pixel_t A, pixel_t B){
    pixel_t C;
    int16_t temp_d = (int16_t) A - (int16_t) B;

    constexpr uint8_t T1 = 32;
    constexpr uint8_t T2 = 96;

    // Note: Since temp_d is unsigned (uint8_t), it can never be < 0.
    // This logic performs a modular subtraction check.
    uint8_t D = (temp_d < 0) ? -temp_d : temp_d;

    if(D < T1) C = (pixel_t) 0;
    else if(D < T2) C = (pixel_t) 128;
    else C = (pixel_t) 255;

    return C;
}
