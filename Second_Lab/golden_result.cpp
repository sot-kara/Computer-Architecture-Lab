#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#define WIDTH 4
#define HEIGHT 4
#define T1 32
#define T2 96

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


void IMAGE_DIFF_POSTERIZE(uint8_t *A , uint8_t *B, uint8_t *out){
    uint8_t C[HEIGHT*WIDTH];
    int temp_filter;
    printf("Difference Matrix:\n");
    for(int i = 0 ; i < HEIGHT ; i++){
        for(int j =0 ; j < WIDTH ; j++){
            int idx = i * WIDTH + j;
            C[idx] = Compare(A[idx], B[idx]);
            printf("%d ", C[idx]);
        }
        printf("\n");
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

    IMAGE_DIFF_POSTERIZE(A, B, out);

    printf("\nResult Output:\n");
    for (int i = 0; i < size; i++)
    {
        printf("%3d ", out[i]);
        if ((i + 1) % WIDTH == 0) printf("\n"); // Format output as matrix
    }

    return 0;
}