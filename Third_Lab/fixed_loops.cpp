#include <stdint.h>
#include <ap_int.h>           // use this type for function i/o and handle as packages

#define WIDTH 64
#define HEIGHT 64
#define BUFFER_SIZE (BUFFER_HEIGHT*BUFFER_WIDTH)
#define DATAWIDTH 512       // Data width of Memory Access in bits
#define BUFFER_HEIGHT 64
#define BUFFER_WIDTH 64
#define CACHE_PAD 2         // Holds the two previous lines columns for correct function of the inner frame filtering.
#define PIXEL_SIZE 8        // pixel size in bits
#define VECTOR_SIZE (DATAWIDTH / PIXEL_SIZE) // vector size is 64 (512/8 = 64 pixels in one 512bit data packet)

typedef ap_uint<DATAWIDTH> uint512_dt;

const int BUFFER_SIZE = BUFFER_HEIGHT*BUFFER_WIDTH;

// TRIPCOUNT identifier
const unsigned int c_size = BUFFER_SIZE;
const unsigned int c_len = HEIGHT*WIDTH / c_size;

uint8_t Compare(uint8_t A, uint8_t B);


extern "C" {
void IMAGE_DIFF_POSTERIZE(const uint512_dt *in_A, const uint512_dt *in_B, uint512_dt *out, unsigned int size)
{
    #pragma HLS INTERFACE m_axi port = in_A offset = slave bundle = gmem
    #pragma HLS INTERFACE m_axi port = in_B offset = slave bundle = gmem
    #pragma HLS INTERFACE m_axi port = out offset = slave bundle = gmem
    #pragma HLS INTERFACE s_axilite port = in_A bundle = control
    #pragma HLS INTERFACE s_axilite port = in_B bundle = control
    #pragma HLS INTERFACE s_axilite port = out bundle = control
    #pragma HLS INTERFACE s_axilite port = size bundle = control
    #pragma HLS INTERFACE s_axilite port = return bundle = control


    uint8_t buffer_A[BUFFER_HEIGHT][BUFFER_WIDTH];
    uint8_t buffer_B[BUFFER_HEIGHT][BUFFER_WIDTH];
    uint8_t cache[BUFFER_HEIGHT][BUFFER_WIDTH]; // Local Memory to store result
    uint8_t out_buffer[BUFFER_HEIGHT-CACHE_PAD][BUFFER_WIDTH-CACHE_PAD];

    // Partitioning
    #pragma HLS ARRAY_PARTITION variable=buffer_A dim=0 type=complete
    #pragma HLS ARRAY_PARTITION variable=buffer_B dim=0 type=complete
    #pragma HLS ARRAY_PARTITION variable=cache dim=0 type=complete
	#pragma HLS ARRAY_PARTITION variable=out_buffer dim=0 type=complete

    int temp_filter;
    int extra_cols = 0;
    int extra_rows = 0;
    int pixel_idx = 0;

    /* Whole & Partial Step Calculations - Logic
        The Logic here is that we take the Initialization step placing our Cache in the top left corner spanning 5 to 5.
        Then in every cache shift we move to the next 5 group BUT KEEPING the last two elements in order for the inner filter to cover the whole area.
        There for we need (1 + how many times our REAL STEP into new elements (BUFFER_SIZE-CACHE_PAD) fits into the left over elements (SIZE - BUFFER_SIZE)
        MODULO informs us if there are leftover elements.
    */
    const bool v_flag =  (HEIGHT - BUFFER_HEIGHT) % (BUFFER_HEIGHT - CACHE_PAD);

    const int v_steps = 1 + (HEIGHT - BUFFER_HEIGHT) / (BUFFER_HEIGHT - CACHE_PAD);

    const bool h_flag = (WIDTH - BUFFER_WIDTH) % (BUFFER_WIDTH - CACHE_PAD);

    const int h_steps = 1 + (WIDTH - BUFFER_WIDTH) / (BUFFER_WIDTH - CACHE_PAD);

    // Reference Point Initialization
    int ref = 0;

    // Caching whole input array
    LINES: for (int v_step = 0; v_step < v_steps + v_flag; v_step++){

        // Last Row Check :
        if(v_step == v_steps){   // handle Leftover Rows:
            extra_rows = (HEIGHT - BUFFER_HEIGHT) % (BUFFER_HEIGHT - CACHE_PAD);
            ref = ((v_steps - 2 + 1)*(BUFFER_HEIGHT - CACHE_PAD))* WIDTH;           // reverting last modify by the main for loop
            ref += extra_rows*WIDTH;
        }

        COLUMNS: for (int h_step = 0; h_step < h_steps + h_flag; h_step++){

            // Last Column Check :
            if(h_step == h_steps){   // handle Leftover Columns:

                extra_cols = (WIDTH - BUFFER_WIDTH) % (BUFFER_WIDTH - CACHE_PAD);

                ref -= BUFFER_WIDTH - CACHE_PAD; // reverting last modify by the main for loop

                ref += extra_cols;
            }

            // ========== MAIN OPERATIONS AREA ==========

            // Caching Input Buffers
            for (int i = 0; i < BUFFER_HEIGHT; i++)
            {
                // Get the data packet that contains the pixels for this row
                uint512_dt data_packet_A = in_A[pixel_idx / VECTOR_SIZE];
                uint512_dt data_packet_B = in_B[pixel_idx / VECTOR_SIZE];
                for (int j = 0; j < BUFFER_WIDTH; j++)
                {
                    int offset = (pixel_idx % VECTOR_SIZE) * 8;
                    buffer_A[i][j] = (data_packet_A >> offset) & 0xFF;
                    buffer_B[i][j] = (data_packet_B >> offset) & 0xFF;
                    pixel_idx++;
                    if ((pixel_idx % VECTOR_SIZE) == 0)
                    {
                        // If we have consumed all pixels in the current data packet, fetch the next one
                        data_packet_A = in_A[pixel_idx / VECTOR_SIZE];
                        data_packet_B = in_B[pixel_idx / VECTOR_SIZE];
                    }
                }
                pixel_idx += (WIDTH - BUFFER_WIDTH);
            }

            // 1) Caching the Difference
            read: for (int j = 0; j < BUFFER_HEIGHT; j++){
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {
                    #pragma HLS UNROLL

                    // Calculate the global row and column indices for the current cache window pixel
                    int row = ref / WIDTH + j;                                                      // NOTE: Possible blunder here with -1 so erased;
                    int col = ref % WIDTH + k;

                    // Caching Compared values                                                      TODO: Check if works better with buffers.
                    cache[j][k] = Compare(buffer_A[j][k], buffer_B[j][k]);
                }
            }

            // 2) Filtering Process (Using only the inner frame)
            for (int inner_row = 0; inner_row < BUFFER_HEIGHT - CACHE_PAD; inner_row++){
                for (int inner_col = 0; inner_col < BUFFER_WIDTH - CACHE_PAD; inner_col++){
                #pragma HLS UNROLL

                    temp_filter = 5 * cache[1+inner_row][1+inner_col]   // center
                                    - cache[inner_row][inner_col+1]     // top
                                    - cache[inner_row+2][inner_col+1]   // bottom
                                    - cache[inner_row+1][inner_col]     // left
                                    - cache[inner_row+1][inner_col+2];  // right

                    //3) Output Logic
                    // ref = index in the linear context of the output/input.
                    // So we're padding by (inner_row +1)*WIDTH to jump to our row in the linear and then (inner_col+1) for the column.

                    out_buffer[inner_row][inner_col] = (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));
                }
            }

            // Writing Output Buffer to Global Memory
            for (int inner_row = 0; inner_row < BUFFER_HEIGHT - CACHE_PAD; inner_row++){
                for (int inner_col = 0; inner_col < BUFFER_WIDTH - CACHE_PAD; inner_col+=VECTOR_SIZE){
					#pragma HLS PIPELINE II=1
                    // Packaging output pixels into uint512_dt format
                    uint512_dt packed_output = 0;
                    for (int pixel = 0; pixel < VECTOR_SIZE; pixel++) {
                        packed_output.range(PIXEL_SIZE * (pixel + 1) - 1, pixel * PIXEL_SIZE) = out_buffer[inner_row][inner_col];
                    }
                	// Write the buffered output
                	out[ref + WIDTH*(inner_row+1) + (inner_col+1)] = packed_output;
                } //out[ref + WIDTH*(inner_row+1) + (inner_col+1)] = out_buffer[inner_row][inner_col];
            }
            // Shifting horizontaly reference point
            ref += BUFFER_WIDTH - CACHE_PAD;
            pixel_idx = ref;
        }

        //Shifting verticaly reference point
        ref = ((v_step + 1)*(BUFFER_HEIGHT - CACHE_PAD))* WIDTH;
    }

      /*  // 4) Clear Border
        for (int row=0; row<HEIGHT; row++){
            if (row==0 || row==HEIGHT-1) {
                for (int col=0; col<WIDTH; col++) {
                #pragma HLS PIPELINE II=1
                    out[row*WIDTH + col] = 0;               // First & Last Rows
                }
            }
            else {
                out[row*WIDTH] = 0;                     // left border
                out[row*WIDTH + WIDTH - 1] = 0;         // right border
            }
        } */
    }
}


uint8_t Compare(uint8_t A, uint8_t B){
    uint8_t C;
    int16_t temp_d = (int16_t) A - (int16_t) B;

    constexpr uint8_t T1 = 10;
    constexpr uint8_t T2 = 50;

    // Note: Since temp_d is unsigned (uint8_t), it can never be < 0.
    // This logic performs a modular subtraction check.
    uint8_t D = (temp_d < 0) ? -temp_d : temp_d;

    if(D < T1) C = 0;
    else if(D < T2) C = 128;
    else C = 255;

    return C;
}
