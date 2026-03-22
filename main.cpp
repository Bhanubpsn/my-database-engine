#include <iostream>
#include "types.h"
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)
#define TABLE_MAX_PAGES 100
using namespace std;


extern const uint32_t ID_SIZE;
extern const uint32_t USERNAME_SIZE;
extern const uint32_t EMAIL_SIZE;
extern const uint32_t ID_OFFSET;
extern const uint32_t USERNAME_OFFSET;
extern const uint32_t EMAIL_OFFSET;
extern const uint32_t ROW_SIZE;
extern const uint32_t PAGE_SIZE;
extern const uint32_t ROWS_PER_PAGE;
extern const uint32_t TABLE_MAX_ROWS;

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
            cout<<"Closing DB connection\n";
            exit(EXIT_SUCCESS);
        } else {
            return META_COMMAND_UNRECOGNIZED_COMMAND;
        }
    }
};

class Table {
public: 
    uint32_t num_rows;              // Number of rows in the table
    void* pages[TABLE_MAX_PAGES];   // Pointers to each page that contains rows

    Table() {
        this->num_rows = 0;
        for(uint32_t i=0; i<TABLE_MAX_PAGES; i++) this->pages[i] = nullptr;
    }
};

// Each row of the table
class Row {
public:
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];

    void serialize_row(void* destination) {
        // Just to give the void pointer 1 byte of space, Clang is strict in these sceneiors 
        char* dest = static_cast<char*>(destination);

        memcpy(dest + ID_OFFSET, &(this->id), ID_SIZE);
        memcpy(dest + USERNAME_OFFSET, &(this->username), USERNAME_SIZE);
        memcpy(dest + EMAIL_OFFSET, &(this->email), EMAIL_SIZE);
    }

    void deserialize_row(void* source) {
        char* src = static_cast<char*>(source);

        memcpy(&(this->id), src + ID_OFFSET, ID_SIZE);
        memcpy(&(this->username), src + USERNAME_OFFSET, USERNAME_SIZE);
        memcpy(&(this->email), src + EMAIL_OFFSET, EMAIL_SIZE);
    }

    void* row_slot(Table* table, uint32_t row_num) {
        uint32_t page_num = row_num / ROWS_PER_PAGE;
        void* page = table->pages[page_num];
        if (page == nullptr) {
            // Allocate memory only when we try to access page
            page = table->pages[page_num] = malloc(PAGE_SIZE);
        }
        uint32_t row_offset = row_num % ROWS_PER_PAGE;
        uint32_t byte_offset = row_offset * ROW_SIZE;

        // giving page, a void pointer, 1 byte space
        return static_cast<char*>(page) + byte_offset;
    }

    void print_row() {
        cout<<this->id<<" "<<this->username<<" "<<this->email<<"\n";
    }
};

// I have serialized the rows, so I need to have the offset for each column
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;
// Storing each row in a serialized blocks of memory , pages.
const uint32_t PAGE_SIZE = 4096;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

class Statement {
public:
    StatementType type;
    Row row_to_insert;

    // Inserting into table and increasing the num of rows count in the table
    ExecuteResult execute_insert(Table* table) {
        if (table->num_rows >= TABLE_MAX_ROWS) {
            return EXECUTE_TABLE_FULL;
        }

        // Serializing the row and inserting into the pages with offset
        row_to_insert.serialize_row(row_to_insert.row_slot(table, table->num_rows));
        table->num_rows++;

        return EXECUTE_SUCCESS;
    }

    // Printing every row in the table
    ExecuteResult execute_select(Table* table) {
        Row* row = new Row();
        for (uint32_t i = 0; i < table->num_rows; i++) {
            row->deserialize_row(row->row_slot(table, i));
            row->print_row();
        }
        delete row;
        return EXECUTE_SUCCESS;
    }

    ExecuteResult execute_statement(Table* table) {
        switch (this->type) {
            case (STATEMENT_INSERT):
                return execute_insert(table);
            case (STATEMENT_SELECT):
                return execute_select(table);
        }
    }

    // Parser for SQL commands
    PrepareResult prepare_statement(char* input_buffer) {
        if (strncasecmp(input_buffer, "insert", 6) == 0) {
            this->type = STATEMENT_INSERT;
            int args_assigned = sscanf(input_buffer, "insert %d %s %s", &(this->row_to_insert.id),this->row_to_insert.username, this->row_to_insert.email);
            if (args_assigned < 3) {
                return PREPARE_SYNTAX_ERROR;
            }
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
    Table* table = new Table();

    // Continously reading the db input
    while(1) {
        inputBuffer->print_prompt();
        inputBuffer->read_input();

        Statement statement;

        if (inputBuffer->buffer[0] == '.') {
            switch (inputBuffer->do_meta_command()) {
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrecognized command '%s'\n", inputBuffer->buffer);
                continue;
            }
        }

        switch (statement.prepare_statement(inputBuffer->buffer)) {
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statement.\n");
                continue;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s'.\n",inputBuffer->buffer);
                continue;
        }

        switch (statement.execute_statement(table)) {
            case (EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case (EXECUTE_TABLE_FULL):
                printf("Error: Table full.\n");
                break;
        }
    }

    // Free up the heap
    delete inputBuffer;
    delete table;

    // To tell the shell that the program executed correctly, or else it would be anything other than 0.
    // Weird right? => How many things went wrong = 0
    return 0;
}
