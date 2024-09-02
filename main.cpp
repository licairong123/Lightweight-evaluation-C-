#include <iostream>

#include "samplemodel.h"

using namespace std;

int main() {
    // Using the function
    int result = add(10, 20);

    // Using the class
    ExampleClass ex;
    int result2 = ex.multiply(10, 20);


    cout << "Result: " << result << endl;
    system("pause");
    
    return 0;
}