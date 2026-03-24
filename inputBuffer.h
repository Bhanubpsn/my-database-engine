#include <iostream>
#include "types.h"
#include "table.h"
#include "b-tree.h"
#ifndef INPUTBUFFER_H
#define INPUTBUFFER_H
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
    MetaCommandResult do_meta_command(Table* table) {
        if (strcmp(this->buffer, ".exit") == 0) {
            table->db_close();
            cout<<"Closing DB connection\n";
            exit(EXIT_SUCCESS);
        } else if (strcmp(this->buffer, ".btree") == 0) {
            printf("Tree:\n");
            void* node = table->pager->get_page(0);
            // LeafNode leafNode;
            // leafNode.node = (uint8_t*)node;
            // leafNode.print_leaf_node();
            InternalNode internalNode;
            internalNode.node = (uint8_t*)node;
            internalNode.print_tree(table->pager,0,0);
            return META_COMMAND_SUCCESS;
        } else {
            return META_COMMAND_UNRECOGNIZED_COMMAND;
        }
    }
};

#endif
