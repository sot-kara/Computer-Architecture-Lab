#include <stdio.h>
#include <stdint.h>

#define WIDTH 512
#define HEIGHT 512
#define OUTPUT_PATH "C:/Users/{USER}/Documents/ref_output.dat" // change this to your prefered path


void IMAGE_DIFF_POSTERIZE(uint8_t A [HEIGHT][WIDTH], uint8_t B[HEIGHT][WIDTH], uint8_t C[HEIGHT][WIDTH]);

int main(){
    uint8_t A[HEIGHT][WIDTH] ;
	uint8_t B[HEIGHT][WIDTH] ;

	uint8_t ref_data[HEIGHT][WIDTH] = {};
    // filling matrices with dummy values
    for(int i=0; i<HEIGHT; i++){

        for(int j=0; j<WIDTH; j++){
            A[i][j] = (uint8_t) (i*j) % 256;
            B[i][j] = (uint8_t) (i/(j+1)) % 256;
        }
    }

	uint8_t C[HEIGHT][WIDTH] = {};

	IMAGE_DIFF_POSTERIZE(A, B, C);

//	for(int i = 0 ; i < HEIGHT ; i++){
//
//		for(int j =0 ; j < WIDTH ; j++){
//			printf("%3d ", C[i][j]);
//		}
//		putchar('\n');
//	}
	
    FILE *ref= fopen(OUTPUT_PATH, "r"); 

    if(ref==NULL){

    FILE *out = fopen(OUTPUT_PATH, "w");
    if (!out) {
        printf("File open error");
        return 1;
    }

    	for(int i = 0 ; i < HEIGHT ; i++){

    		for(int j =0 ; j < WIDTH ; j++){
    			fprintf(out, "%3d ", C[i][j]);
    		}
    		fprintf(out, "\n");
    	}
    fclose(out);

    printf("Reference output from C simulation written.\n");
    return 0;
    }

    for (int i = 0; i < HEIGHT; i++) {
    	for(int j = 0; j< WIDTH; j++ ){
        if (fscanf(ref, "%3d", &ref_data[i][j]) != 1) {
            printf("Error: reference file corrupted at index (%d,%d).\n", i,j);
            fclose(ref);
            return 1;
        }
    	}
    }
    fclose(ref);

    bool match = 1;
    for (int i = 0; i < HEIGHT; i++) {
    	for(int j = 0; j< WIDTH; j++ ){
    		if(C[i][j]!= ref_data[i][j]){
    			printf("Mismatch at index (%d,%d)", i,j);
    			match = 0;
    		}
    	}
    	}
    if(match){
    	printf("Test PASSED!\n");
    }
    else{
    	printf("Test FAILED!\n");
    }
  return 0;

}
