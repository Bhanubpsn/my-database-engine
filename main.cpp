#include <iostream>
using namespace std;

/*
Making REPL
Reach Execute Process Loop
*/

class InputBuffer {
public:
    char* buffer;
    size_t buffer_length;
    size_t input_length;

    // Not making the input buffer as static instance, because we may need to send different requests from different threads
    InputBuffer() {
        this->buffer = nullptr;
        this->buffer_length = 0;
        this->input_length = 0;
    }

    void print_command() {
    
    }
};

// the input arguements are always argc = x + 1, 1 becuase of the run command of the cpp/c file and then the input arguement array
int main(int argc, char *argv[]) {
    InputBuffer* inputBuffer = new InputBuffer();
    // Continously reading the db input
    while(1) {

    }

    // To tell the shell that the program executed correctly, or else it would be anything other than 0.
    // Weird right?
    return 0;
}