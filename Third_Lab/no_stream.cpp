#include <stdint.h>
#include <ap_int.h>    
#include <hls_stream.h>       

// Original Image
#define WIDTH 128
#define HEIGHT 128

// Transaction Definition
#define PIXEL_SIZE 8
#define AXI_WIDTH_BITS 512       // Data width of Memory Access in bits per cycle
#define AXI_WIDTH_BYTES (AXI_WIDTH_BITS / PIXEL_SIZE)

// Buffer - Cached Image
#define BUFFER_HEIGHT 3
#define BUFFER_WIDTH_CHUNKS 2
#define BUFFER_WIDTH_BYTES (BUFFER_WIDTH_CHUNKS * AXI_WIDTH_BYTES)

const int h_steps = (WIDTH - BUFFER_WIDTH_BYTES) / (BUFFER_WIDTH_BYTES) + 1;

// Type Definitions
typedef ap_uint<AXI_WIDTH_BITS> uint512_dt;
typedef ap_uint<PIXEL_SIZE> pixel_t;
typedef enum {
    HOLD_1,
    HOLD_2,
    STREAM
} FilterState;

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
        #pragma HLS INTERFACE m_axi port = in_A offset = slave bundle = gmem
        #pragma HLS INTERFACE m_axi port = in_B offset = slave bundle = gmem
        #pragma HLS INTERFACE m_axi port = out offset = slave bundle = gmem
        #pragma HLS INTERFACE s_axilite port = in_A bundle = control
        #pragma HLS INTERFACE s_axilite port = in_B bundle = control
        #pragma HLS INTERFACE s_axilite port = out bundle = control
        #pragma HLS INTERFACE s_axilite port = size bundle = control
        #pragma HLS INTERFACE s_axilite port = return bundle = control


        pixel_t Prior_chunk_1[BUFFER_WIDTH_BYTES];
        pixel_t Prior_chunk_2[BUFFER_WIDTH_BYTES];
        pixel_t Current_chunk[BUFFER_WIDTH_BYTES];
        pixel_t Filtered_chunk[BUFFER_WIDTH_BYTES];
        pixel_t inter_pixels[HEIGHT];

        #pragma HLS ARRAY_PARTITION variable=Prior_chunk_1 complete
        #pragma HLS ARRAY_PARTITION variable=Prior_chunk_2 complete     
        #pragma HLS ARRAY_PARTITION variable=Current_chunk complete
        #pragma HLS ARRAY_PARTITION variable=Filtered_chunk complete
    
        // Stream to connect Stage 1 of Comparison and Stage 2 of filtering
        hls::stream<uint512_dt> stream_G;
        
        // Reference point for reading input data
        unsigned int href_point = 0;       
        FilterState State_F = HOLD_2; // Initial State for Filter Stage      
    

        // Shift the buffer horizontally
        for (int h_step = 0; h_step < h_steps; h_step++) {
        #pragma HLS DATAFLOW
        #pragma HLS STREAM variable=stream_G depth= HEIGHT * BUFFER_WIDTH_CHUNKS     // TODO: We may need to add some fifo space for safety
           

            for (int row = 0; row < HEIGHT; row++) {

            unsigned int ref_point = (row*WIDTH + href_point)/AXI_WIDTH_BYTES;

            // Loop the buffer and compare over all image rows
                for (int i = 0; i < BUFFER_WIDTH_CHUNKS; i++) {
                #pragma HLS PIPELINE II=1
                
                uint512_dt val1 = in_A[ref_point + i];
                uint512_dt val2 = in_B[ref_point + i];
                uint512_dt res_G;

                // Compute G(in1, in2) on all AXI PIXELS (64 elements) in parallel
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


            // Filter the buffer over rows and output 
                switch (row) {
                    case 0: State_F = HOLD_2; break;
                    case 1: State_F = HOLD_1; break;
                    default: State_F = STREAM; break;
                }

                // Stream Acceptance Logic
                switch (State_F) {
                    case HOLD_1:        // Hold t-1 row at the buffer
                        // Row chunk unpacking
                        for (int chunk = 0; chunk < BUFFER_WIDTH_CHUNKS; chunk++) {
                        #pragma HLS PIPELINE II=1
                            uint512_dt temp_val = stream_G.read();
                            for (int v = 0; v < AXI_WIDTH_BYTES; v++) {
                            #pragma HLS UNROLL
                                Prior_chunk_1[chunk*AXI_WIDTH_BYTES + v] = temp_val.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);
                            }
                        }
                        break;
                        
                    case HOLD_2:        // Hold t-2 row at the buffer
                        // Row chunk unpacking
                        for (int chunk = 0; chunk < BUFFER_WIDTH_CHUNKS; chunk++) {
                        #pragma HLS PIPELINE II=1
                            uint512_dt temp_val = stream_G.read();
                            for (int v = 0; v < AXI_WIDTH_BYTES; v++) {
                            #pragma HLS UNROLL
                                Prior_chunk_2[chunk*AXI_WIDTH_BYTES + v] = temp_val.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);
                            }
                        }
                        break;

                    case STREAM:        // All lines available, normal operation
                        // Row chunk unpacking
                        for (int chunk = 0; chunk < BUFFER_WIDTH_CHUNKS; chunk++) {
                        #pragma HLS PIPELINE II=1
                        uint512_dt temp_val = stream_G.read();
                            for (int v = 0; v < AXI_WIDTH_BYTES; v++) {
                            #pragma HLS UNROLL
                                Current_chunk[chunk*AXI_WIDTH_BYTES + v] = temp_val.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);
                            }
                        }

                        // Filtering Logic
                        if(href_point == 0){
                            // First pixel of the chunk at the left border

                            Filtered_chunk[0] = 0;
                        }
                        else{
                            Filtered_chunk[0] = inter_pixels[row];
                        }
                        for (int col = 1; col < BUFFER_WIDTH_BYTES; col++) {
                        #pragma HLS UNROLL

                            if (col == BUFFER_WIDTH_BYTES - 1){
                                Filtered_chunk[col] = 0;
                            }
                            else {            
                                int16_t temp_filter = 5 * Prior_chunk_1[col] 
                                                        - Current_chunk[col] 
                                                        - Prior_chunk_2[col] 
                                                        - Prior_chunk_1[col - 1]
                                                        - Prior_chunk_1[col + 1];

                                // Clamping the result to [0, 255]
                                pixel_t filtered_pixel = (pixel_t) (temp_filter < 0) ? 0 : (temp_filter > 255 ? 255 : temp_filter);

                                // Write back to output buffer
                                Filtered_chunk[col] = filtered_pixel; 
                                if(col == BUFFER_WIDTH_BYTES/BUFFER_WIDTH_CHUNKS - 1){
                                     // Middle pixel of the chunk
                                    inter_pixels[row] = filtered_pixel;
                                }
                            }                               
                        }

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

                        // Refresh the Chunks
                        for (int v = 0; v < BUFFER_WIDTH_BYTES; v++) {
                        #pragma HLS UNROLL
                            Prior_chunk_2[v] = Prior_chunk_1[v];
                            Prior_chunk_1[v] = Current_chunk[v];
                        }
                        break;
                }
            }
        //Update reference point over columns
        href_point += AXI_WIDTH_BYTES;            
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