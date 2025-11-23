#include <stdio.h>
#include <stdint.h>

#define WIDTH 2
#define HEIGHT 2



void IMAGE_DIFF_POSTERIZE(uint8_t A [HEIGHT][WIDTH], uint8_t B[HEIGHT][WIDTH], uint8_t **C);

int main(){

	uint8_t A[HEIGHT][WIDTH] = {{4,15}, {28,144}};
	uint8_t B[HEIGHT][WIDTH] = {{23, 42},{225,167}};

	uint8_t C[HEIGHT][WIDTH] = {};

	IMAGE_DIFF_POSTERIZE(A, B, (uint8_t**) C);

	for(int i = 0 ; i < 2 ; i++){

		for(int j =0 ; j < 2 ; j++){
			printf("%d ", C[i][j]);
		}
		putchar('\n');
		}

	return 0;
}
