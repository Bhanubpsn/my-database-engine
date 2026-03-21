#include <iostream>
#include "types.h"
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
        // Input End representation 0
        this->buffer[bytes_read - 1] = 0;
    }

    // Parser for META commands
    MetaCommandResult do_meta_command() {
        if (strcmp(this->buffer, ".exit") == 0) {
            exit(EXIT_SUCCESS);
        } else {
            return META_COMMAND_UNRECOGNIZED_COMMAND;
        }
    }
};

class Statement {
public:
    StatementType type;

    void execute_statement() {
        switch (this->type) {
            case (STATEMENT_INSERT):
                cout<<"This is where we would do an insert.\n";
                break;
            case (STATEMENT_SELECT):
                cout<<"This is where we would do a select.\n";
                break;
        }
    }

    // Parser for SQL commands
    PrepareResult prepare_statement(char* input_buffer) {
        if (strncasecmp(input_buffer, "insert", 6) == 0) {
            this->type = STATEMENT_INSERT;
            return PREPARE_SUCCESS;
        }
        if (strcasecmp(input_buffer, "select") == 0) {
            this->type = STATEMENT_SELECT;
            return PREPARE_SUCCESS;
        }
        return PREPARE_UNRECOGNIZED_STATEMENT;
    }
};  

// the input arguements are always argc = x + 1, 1 becuase of the run command of the cpp/c file and then the input arguement array
int main(int argc, char *argv[]) {
    InputBuffer* inputBuffer = new InputBuffer();
    Statement* statement = new Statement();
    // Continously reading the db input
    while(1) {
        inputBuffer->print_prompt();
        inputBuffer->read_input();

        if (inputBuffer->buffer[0] == '.') {
            switch (inputBuffer->do_meta_command()) {
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrecognized command '%s'\n", inputBuffer->buffer);
                continue;
            }
        }

        switch (statement->prepare_statement(inputBuffer->buffer)) {
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s'.\n",inputBuffer->buffer);
                continue;
        }

        statement->execute_statement();
        cout<<"Executed.\n";
    }

    // Free up the heap
    delete statement;
    delete inputBuffer;

    // To tell the shell that the program executed correctly, or else it would be anything other than 0.
    // Weird right? => How many things went wrong = 0
    return 0;
}
