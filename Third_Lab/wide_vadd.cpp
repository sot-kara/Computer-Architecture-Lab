/*******************************************************************************
Description:
    Wide Memory Access Example using ap_uint<Width> datatype
    Description: This is vector addition example to demonstrate Wide Memory
    access of 512bit Datawidth using ap_uint<> datatype which is defined inside
    'ap_int.h' file.
*******************************************************************************/

//Including to use ap_uint<> datatype
#include <ap_int.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 256           // Image Width
#define HEIGHT 256          // Image Height

// Note: BUFFER_WIDTH must be 
#define BUFFER_HEIGHT 12    // Number of rows of 512bit data packets
#define BUFFER_WIDTH 12     // Number of columns of 512bit data packets

#define BUFFER_SIZE (BUFFER_HEIGHT*BUFFER_WIDTH)
#define DATAWIDTH 512       // Data width of Memory Access in bits
#define CACHE_PAD 2       // Holds the two previous lines columns for correct function of the inner frame filtering.
#define PIXEL_SIZE 8        // bits per pixel
#define T1 32
#define T2 96
#define VECTOR_SIZE (DATAWIDTH / PIXEL_SIZE) // vector size is 64 (512/8 = 64 pixels in one 512bit data packet)
typedef ap_uint<DATAWIDTH> uint512_dt;

//TRIPCOUNT identifier
const unsigned int c_chunk_sz = BUFFER_SIZE;
const unsigned int c_size     = VECTOR_SIZE;

/*
    Vector Addition Kernel Implementation using uint512_dt datatype
    Arguments:
        A_in   (input)     --> Input Vector1
        B_in   (input)     --> Input Vector2
        out   (output)    --> Output Vector
        size  (input)     --> Size of Vector in Integer
   */
extern "C"
{
    void vadd(
        const uint512_dt *A_in, // Read-Only Vector 1
        const uint512_dt *B_in, // Read-Only Vector 2
        uint512_dt *out,       // Output Result
        int size               // Size in integer
    )
    {
#pragma HLS INTERFACE m_axi port = A_in bundle = gmem
#pragma HLS INTERFACE m_axi port = B_in bundle = gmem1
#pragma HLS INTERFACE m_axi port = out bundle = gmem2
#pragma HLS INTERFACE s_axilite port = A_in bundle = control
#pragma HLS INTERFACE s_axilite port = B_in bundle = control
#pragma HLS INTERFACE s_axilite port = out bundle = control
#pragma HLS INTERFACE s_axilite port = size bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

        uint512_dt A_local[BUFFER_SIZE]; // Local memory to store A locally
        uint512_dt B_local[BUFFER_SIZE];
        uint512_dt result_local[BUFFER_SIZE]; // Local Memory to store result

        // Input vector size for integer vectors. However kernel is directly
        // accessing 512bit data (total 64 elements). So total number of read
        // from global memory is calculated here:
        int size_in16 = (size - 1) / VECTOR_SIZE + 1; 

        //Per iteration of this loop perform BUFFER_SIZE vector addition
        for (int i = 0; i < size_in16; i += BUFFER_SIZE) {
//#pragma HLS PIPELINE
#pragma HLS DATAFLOW
#pragma HLS stream variable = A_local depth = BUFFER_SIZE
#pragma HLS stream variable = B_local depth = BUFFER_SIZE

            int chunk_size = BUFFER_SIZE;

            //boundary checks
            if ((i + BUFFER_SIZE) > size_in16)
                chunk_size = size_in16 - i;

        //burst read first vector from global memory to local memory
        v1_rd:
            for (int j = 0; j < chunk_size; j++) {
#pragma HLS pipeline
#pragma HLS LOOP_TRIPCOUNT min = 1 max = 64
                A_local[j] = A_in[i + j];
                B_local[j] = B_in[i + j];
            } 

        //burst read second vector and perform vector addition
        add:
			for (int j = 0; j < chunk_size; j++) {
			#pragma HLS pipeline
			#pragma HLS LOOP_TRIPCOUNT min = 1 max = 64
							uint512_dt tmpV1 = v1_local[j];
							uint512_dt tmpV2 = v2_local[j];

							uint512_dt tmpOut = 0;

							for (int vector = 0; vector < VECTOR_SIZE; vector++) {
							   #pragma HLS UNROLL
								ap_uint<PIXEL_SIZE> tmp1 = tmpV1.range(PIXEL_SIZE * (vector + 1) - 1, vector * PIXEL_SIZE);
								ap_uint<PIXEL_SIZE> tmp2 = tmpV2.range(PIXEL_SIZE * (vector + 1) - 1, vector * PIXEL_SIZE);
								tmpOut.range(PIXEL_SIZE * (vector + 1) - 1, vector * PIXEL_SIZE) = tmp1 + tmp2;
                                
							}
                            result_local[j] = tmpOut;
                            
							//out[i + j]       = tmpV1 + tmpV2; // Vector Addition Operation

            }
        store:
            for (int j = 0; j < chunk_size; j++) {
            #pragma HLS pipeline
                    out[i + j] = result_local[j];
            }
        }
    }
}