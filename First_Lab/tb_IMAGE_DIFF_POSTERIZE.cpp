#include <stdio.h>
#include <stdint.h>

#define WIDTH 128
#define HEIGHT 128


void IMAGE_DIFF_POSTERIZE(uint8_t A [HEIGHT][WIDTH], uint8_t B[HEIGHT][WIDTH], uint8_t C[HEIGHT][WIDTH]);

int main(){
    uint8_t A[HEIGHT][WIDTH] ;
	uint8_t B[HEIGHT][WIDTH] ;

    // filling matrices with dummy values
    for(int i=0; i<HEIGHT; i++){

        for(int j=0; j<WIDTH; j++){
            A[i][j] = (uint8_t) (i*j) % 256;
            B[i][j] = (uint8_t) (i/(j+1)) % 256;
        }
    }

	uint8_t C[HEIGHT][WIDTH] = {};

	IMAGE_DIFF_POSTERIZE(A, B, C);

	for(int i = 0 ; i < HEIGHT ; i++){

		for(int j =0 ; j < WIDTH ; j++){
			printf("%3d ", C[i][j]);
		}
		putchar('\n');
	}

	return 0;
}
