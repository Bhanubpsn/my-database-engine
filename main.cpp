#include <iostream>
#include "types.h"
#include "inputBuffer.h"
#include "table.h"
using namespace std;

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

    PrepareResult prepare_insert(char* input_buffer) {
        this->type = STATEMENT_INSERT;

        char* keyword = strtok(input_buffer, " ");
        char* id_string = strtok(nullptr, " ");
        char* username = strtok(nullptr, " ");
        char* email = strtok(nullptr, " ");

        if (id_string == nullptr || username == nullptr || email == nullptr) {
            return PREPARE_SYNTAX_ERROR;
        }

        int id = atoi(id_string);
        if (id < 0) {
            return PREPARE_NEGATIVE_ID;
        }
        if (strlen(username) > COLUMN_USERNAME_SIZE) {
            return PREPARE_STRING_TOO_LONG;
        }
        if (strlen(email) > COLUMN_EMAIL_SIZE) {
            return PREPARE_STRING_TOO_LONG;
        }

        this->row_to_insert.id = id;
        strcpy(this->row_to_insert.username, username);
        strcpy(this->row_to_insert.email, email);

        return PREPARE_SUCCESS;
    }


    // Parser for SQL commands
    PrepareResult prepare_statement(char* input_buffer) {
        if (strncasecmp(input_buffer, "insert", 6) == 0) {
            return prepare_insert(input_buffer);
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
    if (argc < 2) {
        printf("Must supply a database filename.\n");
        exit(EXIT_FAILURE);
    }

    char* filename = argv[1];
    Table* table = new Table(filename);
    InputBuffer* inputBuffer = new InputBuffer();

    // Continously reading the db input
    while(1) {
        inputBuffer->print_prompt();
        inputBuffer->read_input();

        Statement statement;

        if (inputBuffer->buffer[0] == '.') {
            switch (inputBuffer->do_meta_command(table)) {
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
            case (PREPARE_STRING_TOO_LONG):
                printf("String is too long.\n");
                continue;
            case (PREPARE_NEGATIVE_ID):
                printf("ID must be positive.\n");
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
