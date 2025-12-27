/**
 * @file host.cpp
 * @brief OpenCL Host Code for Vector Addition
 *
 * This program coordinates the data transfer and execution of the hardware kernel
 * on the FPGA. It acts as the "Manager" for the FPGA accelerator.
 */

#include "xcl2.hpp" // Xilinx helper functions for OpenCL
#include "event_timer.hpp"
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>  // <--- ADDED: To support file writing
#include <stdint.h>

#define WIDTH  256
#define HEIGHT 256
#define BUFFER_HEIGHT 12
#define BUFFER_WIDTH 12
#define T1 32
#define T2 96

const int DATA_SIZE = WIDTH*HEIGHT;
uint8_t Compare(uint8_t A, uint8_t B);
void IMAGE_DIFF_POSTERIZE(const uint8_t *in_A, const  uint8_t  *in_B, uint8_t *out, unsigned int size);

int main(int argc, char **argv) {

    // -------------------------------------------------------------------------
    // 1. Initial Checks & Setup
    // -------------------------------------------------------------------------
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File>" << std::endl;
        return EXIT_FAILURE;
    }

    EventTimer et;
    std::string binaryFile = argv[1];

    size_t vector_size_bytes = sizeof(uint8_t) * DATA_SIZE;
    int size = DATA_SIZE;

    // OpenCL objects
    cl_int err;
    cl::Context context;
    cl::Kernel krnl_vector_add;
    cl::CommandQueue q;

    // -------------------------------------------------------------------------
    // 2. Host Memory Allocation & Initialization
    // -------------------------------------------------------------------------
    et.add("Allocate Memory in Host Memory");
    std::vector<uint8_t, aligned_allocator<uint8_t>> source_in1(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<uint8_t>> source_in2(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<uint8_t>> source_hw_results(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<uint8_t>> source_sw_results(DATA_SIZE);
    uint8_t in_A[DATA_SIZE];
    uint8_t in_B[DATA_SIZE];
    uint8_t out_C[DATA_SIZE];
    et.finish();

    // Fill vectors with random data
    et.add("Fill the buffers");
    std::generate(source_in1.begin(), source_in1.end(), std::rand);
    std::generate(source_in2.begin(), source_in2.end(), std::rand);

    // Calculate Golden Result (Software Reference)
    for (int i = 0; i < DATA_SIZE; i++) {
    	in_A[i] = source_in1[i];
    	in_B[i] = source_in2[i];
        source_hw_results[i] = 0; // Clear HW result buffer
    }
    IMAGE_DIFF_POSTERIZE(in_A, in_B, out_C, size);
    et.finish();

    std::cout << "A matrix: \n";
    for(int i = 0; i< DATA_SIZE; i++){
    	std::cout << (int)in_A[i] << " ";
    }
    std::cout << "\n";

    std::cout << "B matrix: \n";
    for(int i = 0; i< DATA_SIZE; i++){
    	std::cout << (int)in_B[i] << " ";
    }
    std::cout << "\n";

    std::cout << "C matrix: \n";
    for(int i = 0; i< DATA_SIZE; i++){
    	std::cout << (int)out_C[i] << " ";
    }
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // 3. OpenCL Setup
    // -------------------------------------------------------------------------
    et.add("Load Binary File to Alveo U200");
    auto devices = xcl::get_xil_devices();
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};

    int valid_device = 0;
    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        OCL_CHECK(err, context = cl::Context(device, NULL, NULL, NULL, &err));
        OCL_CHECK(err, q = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));

        std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
        cl::Program program(context, {device}, bins, NULL, &err);

        if (err != CL_SUCCESS) {
            std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
        } else {
            std::cout << "Device[" << i << "]: program successful!\n";
            OCL_CHECK(err, krnl_vector_add = cl::Kernel(program, "IMAGE_DIFF_POSTERIZE", &err));
            valid_device++;
            break;
        }
    }

    if (valid_device == 0) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }
    et.finish();

    // -------------------------------------------------------------------------
    // 4. Device Memory Allocation
    // -------------------------------------------------------------------------
    et.add("Allocate Buffer in Global Memory");
    OCL_CHECK(err, cl::Buffer buffer_in1(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, vector_size_bytes, source_in1.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_in2(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, vector_size_bytes, source_in2.data(), &err));
    OCL_CHECK(err, cl::Buffer buffer_output(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, vector_size_bytes, source_hw_results.data(), &err));
    et.finish();

    // -------------------------------------------------------------------------
    // 5. Kernel Execution
    // -------------------------------------------------------------------------
    et.add("Set the Kernel Arguments");
    OCL_CHECK(err, err = krnl_vector_add.setArg(0, buffer_in1));
    OCL_CHECK(err, err = krnl_vector_add.setArg(1, buffer_in2));
    OCL_CHECK(err, err = krnl_vector_add.setArg(2, buffer_output));
    OCL_CHECK(err, err = krnl_vector_add.setArg(3, size));
    et.finish();

    et.add("Copy input data to device global memory");
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_in1, buffer_in2}, 0));
    et.finish();

    et.add("Launch the Kernel");
    OCL_CHECK(err, err = q.enqueueTask(krnl_vector_add));
    et.finish();

    et.add("Copy Result from Device Global Memory to Host Local Memory");
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_output}, CL_MIGRATE_MEM_OBJECT_HOST));
    OCL_CHECK(err, err = q.finish());
    et.finish();

    // -------------------------------------------------------------------------
    // NEW STEP: Export Results to File
    // -------------------------------------------------------------------------
    // We export before verification so we have the data even if verification fails.
    et.add("Export results to ../results_comparison.txt");

    std::ofstream outFile("../results_comparison.txt");
    if (outFile.is_open()) {
        outFile << "Index\tSW_Result\tHW_Result\tMatch\n";
        for (int i = 0; i < DATA_SIZE; i++) {
            bool is_match = (source_hw_results[i] == out_C[i]);
            outFile << i << "\t"
                    << (int)out_C[i] << "\t"
                    << (int)source_hw_results[i] << "\t"
                    << (is_match ? "YES" : "NO") << "\n";
        }
        outFile.close();
        std::cout << "Successfully wrote results to ../results_comparison.txt" << std::endl;
    } else {
        std::cerr << "Error: Unable to open file ../results_comparison.txt for writing." << std::endl;
    }
    et.finish();

    // -------------------------------------------------------------------------
    // 6. Verification
    // -------------------------------------------------------------------------
    et.add("Compare the results of the Device to the simulation");
    bool match = true;
    for (int i = 0; i < DATA_SIZE; i++) {
        if (source_hw_results[i] != out_C[i]) {
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << " CPU result = " << (int)out_C[i]
                      << " Device result = " << (int)source_hw_results[i] << std::endl;
            match = false;
            break;
        }
    }
    et.finish();

    std::cout << "----------------- Key execution times -----------------" << std::endl;
    et.print();

    std::cout << "TEST " << (match ? "PASSED" : "FAILED") << std::endl;
    return (match ? EXIT_SUCCESS : EXIT_FAILURE);
}

uint8_t Compare(uint8_t A, uint8_t B){
    uint8_t C;
    int16_t temp_d = (int16_t) A - (int16_t) B;
    // Note: Since temp_d is unsigned (uint8_t), it can never be < 0.
    // This logic performs a modular subtraction check.
    uint8_t D = (temp_d < 0) ? -temp_d : temp_d;

    if(D < T1) C = 0;
    else if(D < T2) C = 128; // Note: Overwrites C=0 if T1 <= D < T2
    else C = 255;

    return C;
}

void IMAGE_DIFF_POSTERIZE(const uint8_t *in_A, const uint8_t *in_B, uint8_t *out, unsigned int size)
{

    unsigned int v1_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector1
    unsigned int v2_buffer[BUFFER_HEIGHT][BUFFER_WIDTH];   // Local memory to store vector2
    unsigned int vout_buffer[BUFFER_HEIGHT][BUFFER_WIDTH]; // Local Memory to store result
    int temp_filter;

// Per iteration of this loop perform BUFFER_SIZE vector addition
Chunk_loop:
    for (int i = 0; i < size; i += 1)
    {
        // Handle boundary pixels
        if (i / WIDTH == 0 || i / WIDTH == HEIGHT - 1 ||
            i % WIDTH == 0 || i % WIDTH == WIDTH - 1)
        {

            out[i] = 0; // Center pixel
        }
        else
        {
        read:
            for (int j = 0; j < BUFFER_HEIGHT; j++)
            {
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {

                    // Calculate the global row and column indices for the current window pixel
                    int row = i / WIDTH + j - 1; // Offset by -1 for the 3x3 relative to center
                    int col = i % WIDTH + k - 1;


                    // Safely populate the buffer
                    v1_buffer[j][k] = in_A[row * WIDTH + col];
                    v2_buffer[j][k] = in_B[row * WIDTH + col];
                }
            }

            for (int j = 0; j < BUFFER_HEIGHT; j++)
            {
                for (int k = 0; k < BUFFER_WIDTH; k++)
                {
                    vout_buffer[j][k] = Compare((uint8_t)v1_buffer[j][k], (uint8_t)v2_buffer[j][k]);
                }
            }

        // Perform filter processing
        process:

            // Compute filtered value for the center
            temp_filter = 5 * vout_buffer[1][1] - vout_buffer[0][1] // Top
                          - vout_buffer[2][1]                       // Bottom
                          - vout_buffer[1][0]                       // Left
                          - vout_buffer[1][2];                      // Right

            // Clip result and write to "out" buffer
            out[i] =
                (uint8_t)(temp_filter < 0 ? 0 : (temp_filter > 255 ? 255 : temp_filter));
        }
    }
}


