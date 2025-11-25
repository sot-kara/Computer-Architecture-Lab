#define WIDTH 512
#define HEIGHT 512
#define T1 32
#define T2 96
#include <stdint.h>
#include <math.h>

void IMAGE_DIFF_POSTERIZE(uint8_t A [HEIGHT][WIDTH], uint8_t B[HEIGHT][WIDTH], uint8_t C[HEIGHT][WIDTH]){

	uint8_t D; // difference of pixel values
	int16_t temp_d;

// Partition the arrays so more there can be more access in one iteration with loop unrolling and pipelining
#pragma HLS ARRAY_PARTITION variable=A type=block factor=WIDTH/8 dim=2
#pragma HLS ARRAY_PARTITION variable=B type=block factor=WIDTH/8 dim=2
#pragma HLS ARRAY_PARTITION variable=C type=block factor=WIDTH/8 dim=2

	// iterate through A and B
	for(int i = 0 ; i < HEIGHT ; i++){

		#pragma HLS PIPELINE // pipeline the A and B load operations mainly
		for(int j =0 ; j < WIDTH ; j++){
		#pragma HLS unroll  factor = WIDTH/4 // Loop unroll by WIDTH
			temp_d = A[i][j] - B[i][j];
			D = abs(temp_d); // get the difference of each corresponding pixels
			if(D < T1){
				C[i][j] = 0;
			}
			else if(D >= T1 && D <T2){
				C[i][j] = 128;
			}
			else{
				C[i][j] = 255;
			}
		}
	}

}
