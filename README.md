# Message Processing System (MPS)

MPS is a framework designed to facilitate multithreading in embedded applications by providing an easy and safe way to handle threads and message passing. The framework is built around three primary components:

- **Pool**: Manages a collection of resources or threads.
- **Worker**: Represents an individual thread or processing unit.
- **Message**: The data exchanged between workers.

For detailed information, refer to the `mps.h` and `mps_tutorials.h` files in the repository.

## Building MPS

You can build the MPS project using either CMake or the provided `build.sh` script.

### Using CMake

1. **Install CMake**: Ensure that CMake is installed on your system. You can download it from the [official website](https://cmake.org/download/) or install it using your system's package manager.

2. **Clone the Repository**:
   ```bash
   git clone https://github.com/Alexk-195/mps.git
   cd mps
   ```

3. **Create a Build Directory**:
   ```bash
   mkdir build
   cd build
   ```

4. **Generate Build Files**:
   ```bash
   cmake ..
   ```

5. **Compile the Project**:
   ```bash
   make
   ```

   The compiled binaries will be located in the `build` directory.

### Using the `build.sh` Script

A convenient `build.sh` script is provided to automate the build process. This script performs the steps outlined above.

1. **Make the Script Executable** (if necessary):
   ```bash
   chmod +x build.sh
   ```

2. **Run the Script**:
   ```bash
   ./build.sh
   ```

   The script will build tutorial without CMake using just g++.

## License

This project is licensed under the MIT License. For more details, see the [LICENSE](https://github.com/Alexk-195/mps/blob/main/LICENSE) file in the repository.

---

For any questions or contributions, feel free to open an issue or submit a pull request on the [GitHub repository](https://github.com/Alexk-195/mps). 
