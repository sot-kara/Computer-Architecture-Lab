#include "xcl2.hpp"
#include "event_timer.hpp"
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <stdint.h>

#define WIDTH  256
#define HEIGHT 512

// Transaction Definition
#define PIXEL_SIZE 8 // pixel size in bits
#if WIDTH <= 64
    #define AXI_WIDTH_BITS WIDTH
#else
    #define AXI_WIDTH_BITS 512       // Data width of Memory Access in bits per cycle
#endif

#define VECTOR_SIZE AXI_WIDTH_BITS/PIXEL_SIZE  // 512 bits / 8 bits per pixel

#define T1 32
#define T2 96

const int DATA_SIZE = WIDTH * HEIGHT;
const int PACKET_COUNT = (DATA_SIZE + VECTOR_SIZE - 1) / VECTOR_SIZE;  // Ceiling division

uint8_t Compare(uint8_t A, uint8_t B);
void IMAGE_DIFF_POSTERIZE_SW(const uint8_t *in_A, const uint8_t *in_B, uint8_t *out);

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <XCLBIN File>" << std::endl;
        return EXIT_FAILURE;
    }

    EventTimer et;
    std::string binaryFile = argv[1];

    // Calculate buffer size in PACKETS
    size_t packet_size_bytes = AXI_WIDTH_BITS/PIXEL_SIZE;  // 64 bytes per packet
    size_t buffer_size_bytes = PACKET_COUNT * packet_size_bytes;

    cl_int err;
    cl::Context context;
    cl:: Kernel kernel;
    cl::CommandQueue q;

    // ========== HOST MEMORY ALLOCATION ==========
    et.add("Allocate Memory in Host Memory");

    // Pixel-level buffers for easy initialization
    std::vector<uint8_t, aligned_allocator<uint8_t>> in_A(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<uint8_t>> in_B(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<uint8_t>> hw_result(DATA_SIZE);
    std::vector<uint8_t, aligned_allocator<uint8_t>> sw_result(DATA_SIZE);

    et.finish();

    // ========== INITIALIZE DATA ==========
    et.add("Fill the buffers");
    std::generate(in_A.begin(), in_A.end(), std::rand);
    std::generate(in_B.begin(), in_B.end(), std::rand);
    for(int i=0; i < DATA_SIZE; i++){
    	hw_result[i] = 0;
    	sw_result[i] = 0;
    }
/*
    std::cout << "A matrix contents:\n";
    for(int i=0; i< HEIGHT; i++){
    	for(int j=0; j < WIDTH; j++){
    		std::cout << (int)in_A[i*WIDTH + j] << " ";
    	}
    	std::cout << std::endl;
    }
    std::cout << "B matrix contents:\n";
    for(int i=0; i< HEIGHT; i++){
    	for(int j=0; j < WIDTH; j++){
    		std::cout << (int)in_B[i*WIDTH + j] << " ";
    	}
    	std::cout << std::endl;
    } */
    // Compute software reference
    IMAGE_DIFF_POSTERIZE_SW(in_A. data(), in_B.data(), sw_result.data());
    et.finish();

    // ========== OPENCL SETUP ==========
    et.add("Load Binary File to FPGA");
    auto devices = xcl::get_xil_devices();
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program:: Binaries bins{{fileBuf.data(), fileBuf.size()}};

    bool device_found = false;
    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
        OCL_CHECK(err, q = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));

        std::cout << "Trying to program device[" << i << "]:  "
                  << device.getInfo<CL_DEVICE_NAME>() << std::endl;

        cl::Program program(context, {device}, bins, nullptr, &err);
        if (err != CL_SUCCESS) {
            std::cout << "Failed to program device[" << i << "]\n";
            continue;
        }

        std::cout << "Device[" << i << "]: program successful!\n";
        OCL_CHECK(err, kernel = cl::Kernel(program, "IMAGE_DIFF_POSTERIZE", &err));
        device_found = true;
        break;
    }

    if (! device_found) {
        std::cout << "Failed to program any device, exit!\n";
        return EXIT_FAILURE;
    }
    et.finish();

    // ========== DEVICE BUFFERS ==========
    et.add("Allocate Buffer in Global Memory");

    // Use pixel-sized buffers - OpenCL handles packing
    size_t pixel_buffer_bytes = DATA_SIZE * sizeof(uint8_t);

    OCL_CHECK(err, cl::Buffer buffer_in_A(
        context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
        pixel_buffer_bytes, in_A.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_in_B(
        context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY,
        pixel_buffer_bytes, in_B.data(), &err));

    OCL_CHECK(err, cl::Buffer buffer_out(
        context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
        pixel_buffer_bytes, hw_result.data(), &err));

    et.finish();

    // ========== KERNEL EXECUTION ==========
    et.add("Set Kernel Arguments");
    OCL_CHECK(err, err = kernel.setArg(0, buffer_in_A));
    OCL_CHECK(err, err = kernel.setArg(1, buffer_in_B));
    OCL_CHECK(err, err = kernel.setArg(2, buffer_out));
    et.finish();

    et.add("Copy input data to device");
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_in_A, buffer_in_B}, 0));
    et.finish();

    et.add("Launch Kernel");
    OCL_CHECK(err, err = q. enqueueTask(kernel));
    et.finish();

    et.add("Copy results from device");
    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_out}, CL_MIGRATE_MEM_OBJECT_HOST));
    OCL_CHECK(err, err = q.finish());
    et.finish();

    // ========== EXPORT RESULTS ==========
    et. add("Export results to file");
    std::ofstream outFile("../results_comparison.txt");
    if (outFile.is_open()) {
        outFile << "Index\tRow\tCol\tSW_Result\tHW_Result\tMatch\n";
        for (int i = 0; i < DATA_SIZE; i++) {
            bool match = (sw_result[i] == hw_result[i]);
            outFile << i << "\t"
                    << (i / WIDTH) << "\t"
                    << (i % WIDTH) << "\t"
                    << (int)sw_result[i] << "\t"
                    << (int)hw_result[i] << "\t"
                    << (match ? "YES" : "NO") << "\n";
        }
        outFile.close();
        std::cout << "Results written to ../results_comparison.txt\n";
    }
    et.finish();

    // ========== VERIFICATION ==========
    et.add("Verify results");
    bool match = true;
    int mismatch_count = 0;
    for (int i = 0; i < DATA_SIZE; i++) {
        if (sw_result[i] != hw_result[i]) {
            if (mismatch_count < 10) {  // Print first 10 errors
                std::cout << "Mismatch at i=" << i
                          << " (row=" << i/WIDTH << ", col=" << i%WIDTH << "): "
                          << "SW=" << (int)sw_result[i] << " "
                          << "HW=" << (int)hw_result[i] << std::endl;
            }
            mismatch_count++;
            match = false;
        }
    }
    if (mismatch_count > 10) {
        std::cout << "...  and " << (mismatch_count - 10) << " more mismatches\n";
    }
    et.finish();

    std::cout << "\n----------------- Key execution times -----------------\n";
    et.print();

    std::cout << "\nTEST " << (match ? "PASSED" : "FAILED") << std::endl;
    return (match ? EXIT_SUCCESS : EXIT_FAILURE);
}

uint8_t Compare(uint8_t A, uint8_t B) {
    int16_t temp_d = (int16_t)A - (int16_t)B;
    uint8_t D = (temp_d < 0) ? -temp_d : temp_d;

    if (D < T1) return 0;
    else if (D < T2) return 128;
    else return 255;
}

void IMAGE_DIFF_POSTERIZE_SW(const uint8_t *in_A, const uint8_t *in_B, uint8_t *out) {
    // First pass: compute difference
    uint8_t diff[DATA_SIZE];
    for (int i = 0; i < DATA_SIZE; i++) {
        diff[i] = Compare(in_A[i], in_B[i]);
    }

    // Second pass:  apply filter
    for (int row = 0; row < HEIGHT; row++) {
        for (int col = 0; col < WIDTH; col++) {
            int idx = row * WIDTH + col;

            // Handle borders
            if (row == 0 || row == HEIGHT-1 || col == 0 || col == WIDTH-1) {
                out[idx] = 0;
                continue;
            }

            // 5-point stencil
            int16_t temp = 5 * diff[idx]
                         - diff[(row-1) * WIDTH + col]      // top
                         - diff[(row+1) * WIDTH + col]      // bottom
                         - diff[row * WIDTH + (col-1)]      // left
                         - diff[row * WIDTH + (col+1)];     // right

            out[idx] = (temp < 0) ? 0 : ((temp > 255) ? 255 : temp);
        }
    }
}
