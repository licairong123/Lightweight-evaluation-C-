#include <iostream>
#include <chrono>  // For timing

// Function to perform some computations
void performComputations() {
    // Example computations
    double sum = 0.0;
    for (int i = 0; i < 100000000; ++i) {
        sum += i * 0.5;
    }
}

int main() {
    // Start measuring time
    auto start = std::chrono::high_resolution_clock::now();

    // Call the function where your main computations are performed
    performComputations();

    // End measuring time
    auto end = std::chrono::high_resolution_clock::now();

    // Calculate duration in milliseconds
    std::chrono::duration<double, std::milli> duration_ms = end - start;

    // Output the duration in milliseconds
    std::cout << "Elapsed time: " << duration_ms.count() << " ms" << std::endl;

    return 0;
}
