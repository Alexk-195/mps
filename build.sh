gcc ./src/*.cpp  ./tutorials/*.cpp -DMPS_TRACK_OBJECTS -Isrc -Wpedantic -Wall -Wextra -Wconversion -O0 -lstdc++ -std=c++17 -lpthread -o ./mps_tutorial
