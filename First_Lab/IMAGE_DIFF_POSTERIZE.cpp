#define WIDTH 2
#define HEIGHT 2
#define T1 32
#define T2 96
#include <stdio.h>
#include <stdint.h>
#include <math.h>

void IMAGE_DIFF_POSTERIZE(uint8_t A [HEIGHT][WIDTH], uint8_t B[HEIGHT][WIDTH], uint8_t C[HEIGHT][WIDTH]){

	int16_t D; // difference of pixel values

	// iterate through A and B
	for(int i = 0 ; i < HEIGHT ; i++){

		for(int j =0 ; j < WIDTH ; j++){

			D = abs(A[i][j] - B[i][j]); // get the difference of each corresponding pixels
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
