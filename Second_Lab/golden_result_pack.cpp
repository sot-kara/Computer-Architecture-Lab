#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#define WIDTH 5
#define HEIGHT 5

#define T1 32
#define T2 96

const int DATA_SIZE = WIDTH*HEIGHT;
const int WORD_COUNT = DATA_SIZE / 4 + (DATA_SIZE % 4 != 0 ? 1 : 0); // Assuming 4 bytes per word 

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


void IMAGE_DIFF_POSTERIZE(uint32_t *A , uint32_t *B, uint32_t *out){
    uint8_t C[DATA_SIZE];
    int temp_filter;
    uint8_t temp_out[DATA_SIZE];
    printf("Difference Matrix:\n");
    for(int w=0; w< WORD_COUNT; w++){
            uint32_t word_A = A[w];
            uint32_t word_B = B[w];
            for(int byte_idx = 0; byte_idx < 4; byte_idx++){
                    int idx = w * 4 + byte_idx;
                    if(idx < DATA_SIZE){
                    uint8_t byte_A = (word_A >> (byte_idx * 8)) & 0xFF;
                    uint8_t byte_B = (word_B >> (byte_idx * 8)) & 0xFF;
                    C[idx] = Compare(byte_A, byte_B);
                    printf("%d ", C[idx]);
                    }
    }
}


    for(int i = 0 ; i < HEIGHT ; i++){
        for(int j =0 ; j < WIDTH ; j++){
            int idx = i * WIDTH + j;
            if(i==0|| i == HEIGHT -1 || j ==0 || j == WIDTH -1){
                out[idx] = 0; // Set boundary pixels to 0
            }else{
                temp_filter = 5*C[idx] 
                            - C[idx +1] // Right 
                            - C[idx -1] // Left
                            - C[idx + WIDTH] // Bottom
                            - C[idx - WIDTH]; // Top
                // Clip to [0, 255]
                out[idx] = (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));
            }
        }


}

// Pack output
    for(int w=0; w< WORD_COUNT; w++){
            uint32_t packed_word = 0;
            for(int byte_idx = 0; byte_idx < 4; byte_idx++){
                    int idx = w * 4 + byte_idx;
                    uint8_t byte_out = 0;
                    
                    if(idx < DATA_SIZE){
                    byte_out = out[idx];
                    }
                    else{
                    byte_out = 0; // Padding for out-of-bounds
                    }
                    packed_word |= (byte_out << (byte_idx * 8));
            }
            out[w] = packed_word;
    }

}

int main()
{
    const int size = WIDTH * HEIGHT;
    uint8_t A[size], B[size];
    uint32_t out[WORD_COUNT];

    // Initialize random seed based on current time
    srand(time(NULL)); 

    printf("Populating arrays with random values...\n");

    // Array Populating:
    for (int i = 0; i < size; i++)
    {
        // Generate random number between 0 and 255
       // A[i] = (uint8_t)(rand() % 256); 
       // B[i] = (uint8_t)(rand() % 256);
        A[i] = 100;
        B[i] = 1;
        // Optional: Print input values to verify
        printf("Index %d: A=%d, B=%d\n", i, A[i], B[i]);
    }

    // Clear output
    for(int w=0; w< WORD_COUNT; w++){
        out[w] = 0;
    }

    // pack input
    uint32_t packed_A[WORD_COUNT];
    uint32_t packed_B[WORD_COUNT];
    for(int w=0; w< WORD_COUNT; w++){
            uint32_t word_A = 0;
            uint32_t word_B = 0;
            for(int byte_idx = 0; byte_idx < 4; byte_idx++){
                    int idx = w * 4 + byte_idx;
                    uint8_t byte_A = 0;
                    uint8_t byte_B = 0;
                    if(idx < DATA_SIZE){
                    byte_A = A[idx];
                    byte_B = B[idx];
                    }
                    else{
                    byte_A = 0; // Padding for out-of-bounds
                    byte_B = 0; // Padding for out-of-bounds
                    }
                    word_A |= (byte_A << (byte_idx * 8));
                    word_B |= (byte_B << (byte_idx * 8));
            }
            packed_A[w] = word_A;
            packed_B[w] = word_B;
    }

    IMAGE_DIFF_POSTERIZE(packed_A, packed_B, out);

    // Read unpacked output
    printf("\nResult Output:\n");
    for (int w = 0; w < WORD_COUNT; w++)
    {
        uint32_t packed_word = out[w];
        for (int byte_idx = 0; byte_idx < 4; byte_idx++)
        {
            int idx = w * 4 + byte_idx;
            if (idx < DATA_SIZE)
            {
                uint8_t byte_out = (packed_word >> (byte_idx * 8)) & 0xFF;
                printf("%3d ", byte_out);
            }
        }
    }

    return 0;
}