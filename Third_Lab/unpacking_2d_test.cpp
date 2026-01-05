#include <iostream>
#include <cstdint>

#define DATAWIDTH 32
#define BUFFER_HEIGHT 3
#define BUFFER_WIDTH 3

#define HEIGHT 9          // Image Height
#define WIDTH 9           // Image Width

#define NUM_PACKETS (HEIGHT * WIDTH *8) / DATAWIDTH
#define VECTOR_SIZE (DATAWIDTH / 8) // vector size is 4 (32/8 = 4 pixels in one 32bit data packet)


int main()
{
    uint8_t original[HEIGHT][WIDTH];
    uint32_t packed_data[NUM_PACKETS];
    
    uint8_t uncpacked[BUFFER_HEIGHT][BUFFER_WIDTH];
    
    uint8_t counter = 1;
    uint32_t pack = 0;
    int pack_index = 0;
    for(int i=0; i<HEIGHT; i++){
        for(int j=0; j < WIDTH; j++){
            original[i][j] = counter;
            counter ++;
            pack = pack | (original[i][j] << (((j + i*WIDTH) % VECTOR_SIZE) * 8));
            if( ((j + i*WIDTH + 1) % VECTOR_SIZE) == 0 ){
                packed_data[pack_index] = pack;
                pack = 0;
                pack_index++;
            }
        }
    }

    // Unpack the data
    int pixel_idx = 0;
    for(int i = 0; i < BUFFER_HEIGHT; i++){
        uint32_t data_packet = packed_data[pixel_idx / VECTOR_SIZE];
        for(int j = 0; j < BUFFER_WIDTH; j++){
            int offset = (pixel_idx % VECTOR_SIZE) * 8;
            uncpacked[i][j] = (data_packet >> offset) & 0xFF;
            pixel_idx++;
            if((pixel_idx % VECTOR_SIZE) == 0){
                data_packet = packed_data[pixel_idx / VECTOR_SIZE];
            }
        }
        pixel_idx += (WIDTH - BUFFER_WIDTH);
    }

    // Print results
    std::cout << "Original Data:" << std::endl;
    for(int i=0; i<HEIGHT; i++){
        for(int j=0; j < WIDTH; j++){
            std::cout << static_cast<int>(original[i][j]) << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "Packed Data:" << std::endl;
    for(int i=0; i<NUM_PACKETS; i++){
        std::cout << std::hex << packed_data[i] << " ";
    }
    std::cout << std::dec << std::endl;


    std::cout << "Unpacked Data:" << std::endl;
    for(int i=0; i<BUFFER_HEIGHT; i++){
        for(int j=0; j < BUFFER_WIDTH; j++){
            std::cout << static_cast<int>(uncpacked[i][j]) << " ";
        }
        std::cout << std::endl;
    }


    return 0;
}