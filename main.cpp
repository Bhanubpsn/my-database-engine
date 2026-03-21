#include <iostream>
using namespace std;

/*
Making REPL
Read Execute Process Loop
*/

class InputBuffer {
public:
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;

    // Not making the input buffer as static instance, because we may need to send different requests from different threads
    InputBuffer() {
        this->buffer = nullptr;
        this->buffer_length = 0;
        this->input_length = 0;
    }

    void print_prompt() { cout<<"BpsnDB >> "; }

    void read_input() {
        //Fun fact: ssize_t is a fancy way to represent long
        ssize_t bytes_read = getline(&(this->buffer), &(this->buffer_length), stdin);
        if (bytes_read <= 0) {
            cout<<"Error reading input\n";
            exit(EXIT_FAILURE);
        }
        // Ignore trailing newline
        this->input_length = bytes_read - 1;
        this->buffer[bytes_read - 1] = 0;
    }
};

// the input arguements are always argc = x + 1, 1 becuase of the run command of the cpp/c file and then the input arguement array
int main(int argc, char *argv[]) {
    InputBuffer* inputBuffer = new InputBuffer();
    // Continously reading the db input
    while(1) {
        inputBuffer->print_prompt();
        inputBuffer->read_input();

        if (strcmp(inputBuffer->buffer, ".exit") == 0) {
            free(inputBuffer);
            exit(EXIT_SUCCESS);
        } else if (!inputBuffer->input_length) {
            // Foe handling if when the user enters a new line only. i.e. just enter
            continue;
        } else {
            printf("Unrecognized command '%s'.\n", inputBuffer->buffer);
        }
    }

    // To tell the shell that the program executed correctly, or else it would be anything other than 0.
    // Weird right? => How many things went wrong = 0
    return 0;
}