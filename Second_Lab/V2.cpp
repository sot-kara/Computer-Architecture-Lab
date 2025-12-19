#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> 
#include <time.h> 

#define WIDTH 16
#define HEIGHT 16
#define BUFFER_HEIGHT 5
#define BUFFER_WIDTH 5
#define CACHE_PAD 2         // Holds the two previous lines columns for correct function of the inner frame filtering.
#define T1 32
#define T2 96

const int BUFFER_SIZE = BUFFER_HEIGHT * BUFFER_WIDTH;

// TODO: Fix explicitly fill the border even though it is correct somehow???
// TODO: FLAG HANDLING LOGIC
// TODO: Fix Flag raising logic


//  For Lab 3:
// #include "ap_int.h"           // use this type for function i/o and handle as packages
// typedef ap_uint<512> uint512_dt;

// TRIPCOUNT identifier
const unsigned int c_size = BUFFER_SIZE;
const unsigned int c_len = HEIGHT * WIDTH / c_size;

uint8_t Compare(uint8_t A, uint8_t B);

/* Default I/O implies 32 bit AXI4-DATA_WIDTH. Therefore we're starting with one integer read per cycle. */

void IMAGE_DIFF_POSTERIZE(const uint8_t *in_A, const uint8_t *in_B, uint8_t *out, unsigned int size)
{
    uint8_t v1_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector1
    uint8_t v2_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector2
    uint8_t cache[BUFFER_HEIGHT][BUFFER_WIDTH]; // Local Memory to store result
    int temp_filter;
    int extra_cols = 0;
    int extra_rows = 0;

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
    LINES: for (int v_step = 0; v_step < v_steps; v_step++){

        COLUMNS: for (int h_step = 0; h_step < h_steps; h_step++){

            // MAIN OPERATIONS AREA

            // 1) Caching the Difference
            read: for (int j = 0; j < BUFFER_HEIGHT; j++){
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {
                    // Calculate the global row and column indices for the current cache window pixel
                    int row = ref / WIDTH + j;                                                      // NOTE: Possible blunder here with -1 so erased;
                    int col = ref % WIDTH + k;

                    // Caching Compared values                                                      TODO: Check if works better with buffers.
                    cache[j][k] = Compare(in_A[row * WIDTH + col], in_B[row * WIDTH + col]);
                }
            }

            // 2) Filtering Process (Using only the inner frame)    
            
            for (int inner_row = 0; inner_row < BUFFER_HEIGHT - CACHE_PAD; inner_row++){
                for (int inner_col = 0; inner_col < BUFFER_WIDTH - CACHE_PAD; inner_col++){

                    temp_filter = 5 * cache[1+inner_row][1+inner_col]   // center
                                    - cache[inner_row][inner_col+1]     // top
                                    - cache[inner_row+2][inner_col+1]   // bottom
                                    - cache[inner_row+1][inner_col]     // left
                                    - cache[inner_row+1][inner_col+2];  // right
                    
                    //3) Output Logic
                    // ref = index in the linear context of the output/input. 
                    // So we're padding by (inner_row +1)*WIDTH to jump to our row in the linear and then (inner_col+1) for the column.   
                    out[ref + WIDTH*(inner_row+1) + (inner_col+1)] = (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));
                }
            }
            // Shifting horizontaly reference point
            ref += BUFFER_WIDTH - CACHE_PAD;
        }          
        
        last_column_process:  if(h_flag){   // handle h_flag (Leftover Columns):

           extra_cols = (WIDTH - BUFFER_WIDTH) % (BUFFER_WIDTH - CACHE_PAD);
           //printf("Column ref before last proc: %d\n", ref);
           ref -= BUFFER_WIDTH - CACHE_PAD; // reverting last modify by the main for loop
           //printf("Column ref revert: %d\n", ref);
           ref += extra_cols;
           //printf("Column ref extra cols: %d\n", ref);

        // 1) Caching the Difference
            for (int j = 0; j < BUFFER_HEIGHT; j++){
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {
                    // Calculate the global row and column indices for the current cache window pixel
                    int row = ref / WIDTH + j;                                                      // NOTE: Possible blunder here with -1 so erased;
                    int col = ref % WIDTH + k;

                    // Caching Compared values                                                      TODO: Check if works better with buffers.
                    cache[j][k] = Compare(in_A[row * WIDTH + col], in_B[row * WIDTH + col]);
                }
            }

            // 2) Filtering Process (Using only the inner frame)              
            for (int inner_row = 0; inner_row < BUFFER_HEIGHT - CACHE_PAD; inner_row++){
                for (int inner_col = 0; inner_col < BUFFER_WIDTH - CACHE_PAD; inner_col++){

                    temp_filter = 5 * cache[1+inner_row][1+inner_col]   // center
                                    - cache[inner_row][inner_col+1]     // top
                                    - cache[inner_row+2][inner_col+1]   // bottom
                                    - cache[inner_row+1][inner_col]     // left
                                    - cache[inner_row+1][inner_col+2];  // right
                    
                    //3) Output Logic  
                    out[ref + WIDTH*(inner_row+1) + (inner_col+1)] = (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));
                }
            }
        }        
        
        //Shifting verticaly reference point
        ref = ((v_step + 1)*(BUFFER_HEIGHT - CACHE_PAD))* WIDTH;
    }
    
    last_row_process:  if(v_flag){   // handle v_flag (Leftover Rows):

        extra_rows = (HEIGHT - BUFFER_HEIGHT) % (BUFFER_HEIGHT - CACHE_PAD);
        //printf("Column ref before last proc: %d\n", ref);
        ref = ((v_steps - 2 + 1)*(BUFFER_HEIGHT - CACHE_PAD))* WIDTH; // reverting last modify by the main for loop
        //printf("Column ref revert: %d\n", ref);
        ref += extra_rows*WIDTH;
        //printf("Column ref extra cols: %d\n", ref);

        for (int h_step = 0; h_step < h_steps; h_step++){

            // MAIN OPERATIONS AREA

            // 1) Caching the Difference
            for (int j = 0; j < BUFFER_HEIGHT; j++){
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {
                    // Calculate the global row and column indices for the current cache window pixel
                    int row = ref / WIDTH + j;                                                      // NOTE: Possible blunder here with -1 so erased;
                    int col = ref % WIDTH + k;

                    // Caching Compared values                                                      TODO: Check if works better with buffers.
                    cache[j][k] = Compare(in_A[row * WIDTH + col], in_B[row * WIDTH + col]);
                }
            }

            // 2) Filtering Process (Using only the inner frame)    
            
            for (int inner_row = 0; inner_row < BUFFER_HEIGHT - CACHE_PAD; inner_row++){
                for (int inner_col = 0; inner_col < BUFFER_WIDTH - CACHE_PAD; inner_col++){

                    temp_filter = 5 * cache[1+inner_row][1+inner_col]   // center
                                    - cache[inner_row][inner_col+1]     // top
                                    - cache[inner_row+2][inner_col+1]   // bottom
                                    - cache[inner_row+1][inner_col]     // left
                                    - cache[inner_row+1][inner_col+2];  // right
                    
                    //3) Output Logic
                    // ref = index in the linear context of the output/input. 
                    // So we're padding by (inner_row +1)*WIDTH to jump to our row in the linear and then (inner_col+1) for the column.   
                    out[ref + WIDTH*(inner_row+1) + (inner_col+1)] = (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));
                }
            }
            // Shifting horizontaly reference point
            ref += BUFFER_WIDTH - CACHE_PAD;
        }          
        
        last_column_row_process:  if(h_flag){   // handle h_flag (Leftover Columns):

           extra_cols = (WIDTH - BUFFER_WIDTH) % (BUFFER_WIDTH - CACHE_PAD);
           //printf("Column ref before last proc: %d\n", ref);
           ref -= BUFFER_WIDTH - CACHE_PAD; // reverting last modify by the main for loop
           //printf("Column ref revert: %d\n", ref);
           ref += extra_cols;
           //printf("Column ref extra cols: %d\n", ref);

        // 1) Caching the Difference
            for (int j = 0; j < BUFFER_HEIGHT; j++){
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {
                    // Calculate the global row and column indices for the current cache window pixel
                    int row = ref / WIDTH + j;                                                      // NOTE: Possible blunder here with -1 so erased;
                    int col = ref % WIDTH + k;

                    // Caching Compared values                                                      TODO: Check if works better with buffers.
                    cache[j][k] = Compare(in_A[row * WIDTH + col], in_B[row * WIDTH + col]);
                }
            }

            // 2) Filtering Process (Using only the inner frame)              
            for (int inner_row = 0; inner_row < BUFFER_HEIGHT - CACHE_PAD; inner_row++){
                for (int inner_col = 0; inner_col < BUFFER_WIDTH - CACHE_PAD; inner_col++){

                    temp_filter = 5 * cache[1+inner_row][1+inner_col]   // center
                                    - cache[inner_row][inner_col+1]     // top
                                    - cache[inner_row+2][inner_col+1]   // bottom
                                    - cache[inner_row+1][inner_col]     // left
                                    - cache[inner_row+1][inner_col+2];  // right
                    
                    //3) Output Logic  
                    out[ref + WIDTH*(inner_row+1) + (inner_col+1)] = (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));
                }
            }
        }
    } 
}

uint8_t Compare(uint8_t A, uint8_t B)
{
    uint8_t C;
    int16_t temp_d = (int16_t)A - (int16_t)B;

    // Absolute difference logic
    uint8_t D = (temp_d < 0) ? (uint8_t)-temp_d : (uint8_t)temp_d;

    if (D < T1)
        C = 0;
    else if (D < T2)
        C = 128;
    else
        C = 255;

    return C;
}

int main()
{
    const int size = WIDTH * HEIGHT;
    uint8_t A[size], B[size];
    uint8_t out[size];

    // Initialize random seed based on current time
    srand(time(NULL)); 

    printf("Populating arrays with random values...\n");

    // Array Populating:
    for (int i = 0; i < size; i++)
    {
        // Generate random number between 0 and 255
        A[i] = (uint8_t)(rand() % 256); 
        B[i] = (uint8_t)(rand() % 256);
        out[i] = 0;
        
        // Optional: Print input values to verify
        printf("Index %d: A=%d, B=%d\n", i, A[i], B[i]);
    }

    IMAGE_DIFF_POSTERIZE(A, B, out, size);

    printf("\nResult Output:\n");
    for (int i = 0; i < size; i++)
    {
        printf("%3d ", out[i]);
        if ((i + 1) % WIDTH == 0) printf("\n"); // Format output as matrix
    }

    return 0;
}