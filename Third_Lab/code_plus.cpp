#include <stdint.h>
#include <ap_int.h>           

// Original Image
#define WIDTH 64
#define HEIGHT 64

// Transaction Definition
#define PIXEL_SIZE 8
#define AXI_WIDTH_BITS 512       // Data width of Memory Access in bits per cycle
#define AXI_WIDTH_BYTES (AXI_WIDTH_BITS / PIXEL_SIZE)

// Buffer - Cached Image
#define BUFFER_HEIGHT 3
#define BUFFER_WIDTH_CHUNKS 2
#define BUFFER_WIDTH_BYTES (BUFFER_WIDTH_CHUNKS * AXI_WIDTH_BYTES)
#define CACHE_PAD 2          // Holds the two previous lines columns for correct function of the inner frame filtering

const int h_flag  = (WIDTH - BUFFER_WIDTH_BYTES) % (BUFFER_WIDTH_BYTES - CACHE_PAD);
const int h_steps = (WIDTH - BUFFER_WIDTH_BYTES) / (BUFFER_WIDTH_BYTES - CACHE_PAD) + 1;

// Type Definitions
typedef ap_uint<AXI_WIDTH_BITS> uint512_dt;
typedef ap_uint<PIXEL_SIZE> pixel_t;
typedef enum {
    HOLD_1,
    HOLD_2,
    STREAM
} FilterState;

// Helper Functions
uint8_t Compare(uint8_t A, uint8_t B);
void compute_G_stage(const uint512_dt *in1, const uint512_dt *in2, hls::stream<uint512_dt> &stream_G,  unsigned int ref_point);
void compute_F_stage(hls::stream<uint512_dt> &stream_G, uint512_dt *out);

// TRIPCOUNT identifier
const unsigned int BUFFER_SIZE = BUFFER_HEIGHT*BUFFER_WIDTH_BYTES;
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
    
        // Stream to connect Stage 1 of Comparison and Stage 2 of filtering
        hls::stream<uint512_dt> stream_G;
        
        // Reference point for reading input data
        unsigned int href_point = 0;       
        FilterState State_F = HOLD_2; // Initial State for Filter Stage      
        
        // Calculate total horizontal steps
        const int total_h_steps; 
        if (h_steps == 1) total_h_steps = 1;
        else total_h_steps = h_steps + (h_flag ? 1 : 0);

        // Shift the buffer horizontally
        for (int h_step = 0; h_step < total_h_steps; h_step++) {
        #pragma HLS DATAFLOW
        #pragma HLS STREAM variable=stream_G depth=BUFFER_WIDTH_CHUNKS     // TODO: We may need to add some fifo space for safety
           
            // Handle h_flag
            if (h_step == h_steps) {
                href_point -= BUFFER_WIDTH_BYTES - CACHE_PAD;   // Reverting last modify by the for loop
                href_point += h_flag;                           // Fit to the leftover columns
            }

            // Loop the buffer and compare over all image rows
            for (int row = 0; row < HEIGHT; row++) {
                compute_G_stage(in_A, in_B, stream_G, (row*WIDTH + href_point));
            }

            // Filter the buffer over rows and output 
            for (int row = 0; row < HEIGHT; row++) {
                case (row) {
                    case 0: State_F = HOLD_2; break;
                    case 1: State_F = HOLD_1; break;
                    default: State_F = STREAM; break;
                }
                compute_F_stage(stream_G, out, State_F); 
            }

        //Update reference point over columns
        href_point += BUFFER_WIDTH_BYTES - CACHE_PAD;            
        }
    }
}


/* F-STAGE: FILTERING
    - Input  : Stream of compared rows from G stage
    - Output : Filtered output written to global memory
*/
void compute_F_stage(hls::stream<uint512_dt> &stream_G, uint512_dt *out, FilterState &state_F, unsigned int ref_point) {
    pixel_t Prior_chunk_1[BUFFER_WIDTH_BYTES];
    pixel_t Prior_chunk_2[BUFFER_WIDTH_BYTES];
    pixel_t Current_chunk[BUFFER_WIDTH_BYTES];
    pixel_t Filterd_chunk[BUFFER_WIDTH_BYTES];

    #pragma HLS ARRAY_PARTITION variable=Prior_chunk_1 complete
    #pragma HLS ARRAY_PARTITION variable=Prior_chunk_2 complete     
    #pragma HLS ARRAY_PARTITION variable=Current_chunk complete
    #pragma HLS ARRAY_PARTITION variable=Filterd_chunk complete

    // Stream Acceptance Logic
    case (state_F) {
        case HOLD_1:        // Hold t-1 row at the buffer
            // Row chunk unpacking
            for (int v = 0; v < BUFFER_WIDTH_BYTES; v++) {
            #pragma HLS UNROLL
                Prior_chunk_1[v] = stream_G.read().range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);
            }
            break;
            
        case HOLD_2:        // Hold t-2 row at the buffer
            // Row chunk unpacking
            for (int v = 0; v < BUFFER_WIDTH_BYTES; v++) {
            #pragma HLS UNROLL
                Prior_chunk_2[v] = stream_G.read().range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);
            }
            break;

        case STREAM:        // All lines available, normal operation
            // Row chunk unpacking
            for (int v = 0; v < BUFFER_WIDTH_BYTES; v++) {      // maybe that has a cycle delay?
            #pragma HLS UNROLL
                Current_chunk[v] = stream_G.read().range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE);
            }

            // Filtering Logic
            for (int col = 0; col < BUFFER_WIDTH_BYTES; col++) {
            #pragma HLS UNROLL
                
                int16_t temp_filter = 5 * Prior_chunk_1[col] 
                                    - Current_chunk[col] 
                                    - Prior_chunk_2[col] 
                                    - Prior_chunk_1[col - 1]
                                    - Prior_chunk_1[col + 1];

                // Clamping the result to [0, 255]
                pixel_t filtered_pixel = (temp_filter < 0) ? 0 : (temp_filter > 255 ? 255 : (pixel_t)temp_filter);

                // Write back to output buffer
                Filtered_chunk[col] = filtered_pixel;                
            }

            // Write the filtered chunk to output stream
            uint512_dt out_val;
            
            for (int chunk = 0; chunk < BUFFER_WIDTH_CHUNKS; chunk++) {

                // Output packing
                for (int v = 0; v < AXI_WIDTH_BYTES; v++) {
                #pragma HLS UNROLL
                    out_val.range(PIXEL_SIZE * (v + 1) - 1, v * PIXEL_SIZE) = Filtered_chunk[chunk*AXI_WIDTH_BYTES + v];
                }
                
                out[ref_point + chunk*AXI_WIDTH_BYTES] = out_val;
            }

            // TODO: Refresh the Chunks
            break;

        default:        // Not sure for this
            state_F = STREAM;
            break;
    }
}


/* G-STAGE: ROW COMPARISON
    - Input  : 1 row of [BUFFER_WIDTH_CHUNKS] chunks from in_A and in_B right after the ref_point
    - Output : 1 compared row of afforementioned size as input to the filter stage
*/
void compute_G_stage(const uint512_dt *in1, const uint512_dt *in2, hls::stream<uint512_dt> &stream_G,  unsigned int ref_point) {

    for (int i = 0; i < BUFFER_WIDTH_CHUNKS; i++) {
        #pragma HLS PIPELINE II=1
        
        uint512_dt val1 = in1[ref_point + i*AXI_WIDTH_BYTES];
        uint512_dt val2 = in2[ref_point + i*AXI_WIDTH_BYTES];
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