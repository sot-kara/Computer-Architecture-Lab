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

#define WIDTH 256
#define HEIGHT 256
#define T1 32
#define T2 96

const int DATA_SIZE = WIDTH*HEIGHT;
uint8_t Compare(uint8_t A, uint8_t B);

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
    std::vector<uint8_t, aligned_allocator<int>> source_in1(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<int>> source_in2(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<int>> source_hw_results(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<int>> source_sw_results(DATA_SIZE);
    et.finish();

    // Fill vectors with random data
    et.add("Fill the buffers");
    std::generate(source_in1.begin(), source_in1.end(), std::rand);
    std::generate(source_in2.begin(), source_in2.end(), std::rand);
    
    // Calculate Golden Result (Software Reference)
    for (int i = 0; i < DATA_SIZE; i++) {
        source_sw_results[i] = Compare((uint8_t) source_in1[i], (uint8_t) source_in2[i]);
        source_hw_results[i] = 0; // Clear HW result buffer
    }
    et.finish();

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
            bool is_match = (source_hw_results[i] == source_sw_results[i]);
            outFile << i << "\t" 
                    << source_sw_results[i] << "\t" 
                    << source_hw_results[i] << "\t" 
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
        if (source_hw_results[i] != source_sw_results[i]) {
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << " CPU result = " << source_sw_results[i]
                      << " Device result = " << source_hw_results[i] << std::endl;
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