#include <grpcpp/grpcpp.h>
#include "database.grpc.pb.h" // Generated from proto
#include <iostream>
#include "types.h"
#include "inputBuffer.h"
#include "table.h"
#include "cursor.h"
#include "row.h"
#include "b-tree.h"
using namespace std;

void* cursor_value(Cursor* cursor) {
    uint32_t page_num = cursor->page_num;
    // void* page = table->pages[page_num];
    // if (page == nullptr) {
    //     // Allocate memory only when we try to access page
    //     page = table->pages[page_num] = malloc(PAGE_SIZE);
    // }

    //Reading from pager now
    void* page = cursor->table->pager->get_page(page_num);
    LeafNode leafNode;
    leafNode.node = (uint8_t*)page;

    return leafNode.leaf_node_value(cursor->cell_num);
}

class Statement {
public:
    StatementType type;
    Row row_to_insert;

    // Inserting into table and increasing the num of rows count in the table
    ExecuteResult execute_insert(Table* table) {
        void* node = table->pager->get_page(table->root_page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)node;
        uint32_t num_cells = (*leafNode.leaf_node_num_cells());

        uint32_t key_to_insert = row_to_insert.id;
        Cursor* cursor = new Cursor();
        // cursor->table_end(table);
        cursor->table_find(table, key_to_insert);   // Finding the a place to insert the key rather than pushing at the end

        if (cursor->cell_num < num_cells) {
            uint32_t key_at_index = *leafNode.leaf_node_key(cursor->cell_num);
            if (key_at_index == key_to_insert) {
                return EXECUTE_DUPLICATE_KEY;
            }
        }
        cursor->leaf_node_insert(row_to_insert.id, row_to_insert);

        delete cursor;
        return EXECUTE_SUCCESS;
    }

    ExecuteResult execute_update(Table* table) {
        uint32_t key_to_update = row_to_insert.id;
        Cursor* cursor = new Cursor();
        
        // Finding where the key should be
        cursor->table_find(table, key_to_update);

        // Accessing the specific page the cursor found
        void* node = table->pager->get_page(cursor->page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)node;

        uint32_t num_cells = *leafNode.leaf_node_num_cells();

        // Verifying the key actually exists
        if (cursor->cell_num < num_cells) {
            uint32_t key_at_index = *leafNode.leaf_node_key(cursor->cell_num);
            if (key_at_index == key_to_update) {
                // Overwriting the existing row data
                row_to_insert.serialize_row(leafNode.leaf_node_value(cursor->cell_num));
                return EXECUTE_SUCCESS;
            }
        }

        delete cursor;
        return EXECUTE_KEY_NOT_FOUND;
    }

    ExecuteResult execute_delete(Table* table) {
        uint32_t key_to_delete = row_to_insert.id;      // TODO: This name is misleading, we are deleting the row, will change it soon.
        Cursor* cursor = new Cursor();
        
        // Finding where the key should be
        cursor->table_find(table, key_to_delete);

        // Accessing the specific page the cursor found
        void* node = table->pager->get_page(cursor->page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)node;

        uint32_t num_cells = *leafNode.leaf_node_num_cells();

        // Verifying the key actually exists
        if (cursor->cell_num < num_cells) {
            uint32_t key_at_index = *leafNode.leaf_node_key(cursor->cell_num);
            if (key_at_index == key_to_delete) {
                // deleting the node data
                cursor->leaf_node_delete(key_to_delete);
                return EXECUTE_SUCCESS;
            }
        }

        delete cursor;
        return EXECUTE_KEY_NOT_FOUND;
    }

    // Printing every row in the table
    ExecuteResult execute_select(Table* table) {
        Cursor* cursor = new Cursor();
        cursor->table_start(table);

        Row* row = new Row();

        while (!(cursor->end_of_table)) {
            row->deserialize_row(cursor_value(cursor));
            row->print_row();
            cursor->cursor_advance();
        }

        delete row;
        delete cursor;
        return EXECUTE_SUCCESS;
    }

    ExecuteResult execute_statement(Table* table) {
        switch (this->type) {
            case (STATEMENT_INSERT):
                return execute_insert(table);
            case (STATEMENT_SELECT):
                return execute_select(table);
            case (STATEMENT_UPDATE):
                return execute_update(table);
            case (STATEMENT_DELETE):
                return execute_delete(table);
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

    PrepareResult prepare_update(char* input_buffer) {
        this->type = STATEMENT_UPDATE;

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

    PrepareResult prepare_delete(char* input_buffer) {
        this->type = STATEMENT_DELETE;

        char* keyword = strtok(input_buffer, " ");
        char* id_string = strtok(nullptr, " ");

        if (id_string == nullptr) {
            return PREPARE_SYNTAX_ERROR;
        }

        int id = atoi(id_string);
        if (id < 0) {
            return PREPARE_NEGATIVE_ID;
        }
        this->row_to_insert.id = id;    // This name is misleading for this function
        return PREPARE_SUCCESS;
    }


    // Parser for SQL commands
    PrepareResult prepare_statement(char* input_buffer) {
        if (strncasecmp(input_buffer, "insert", 6) == 0) {
            return prepare_insert(input_buffer);
        }
        if (strncasecmp(input_buffer, "update", 6) == 0) {
            return prepare_update(input_buffer);
        }
        if (strcasecmp(input_buffer, "select") == 0) {
            this->type = STATEMENT_SELECT;
            return PREPARE_SUCCESS;
        }
        if (strncasecmp(input_buffer, "delete", 6) == 0) {
            return prepare_delete(input_buffer);
        }
        return PREPARE_UNRECOGNIZED_STATEMENT;
    }
};  

// ProtoBuffer 
class DatabaseEngineImpl final : public bpsndb::DatabaseEngine::Service {
    Table* table;
public:
    DatabaseEngineImpl(Table* t) : table(t) {}
    grpc::Status Execute(grpc::ServerContext* context, const bpsndb::CommandRequest* request, bpsndb::CommandResponse* response) override {
        
        string statement_text = request->statement();
        
        InputBuffer input_buffer;
        input_buffer.buffer = (char*)statement_text.c_str();
        input_buffer.input_length = statement_text.length();

        if (input_buffer.buffer[0] == '.') {
            switch (input_buffer.do_meta_command(table)) {
                case (META_COMMAND_SUCCESS):
                    return grpc::Status::OK;
                case (META_COMMAND_UNRECOGNIZED_COMMAND): {
                    std::string msg = "Unrecognized command: " + std::string(input_buffer.buffer);
                    response->set_message(msg); 
                    return grpc::Status::OK;
                }
            }
        }

        Statement statement;
        switch (statement.prepare_statement(input_buffer.buffer)) {
            case PREPARE_SUCCESS:
                break;
            default:
                response->set_status(bpsndb::CommandResponse::FAILURE);
                response->set_message("Syntax Error");
                return grpc::Status::OK;
        }

        if (statement.type == STATEMENT_SELECT) {
            Cursor* cursor = new Cursor();
            cursor->table_start(table);
            while (!(cursor->end_of_table)) {
                Row row;
                row.deserialize_row(cursor_value(cursor));
                row.print_row();
                
                // Map C++ Row to Proto Row
                bpsndb::Row* proto_row = response->add_rows();
                proto_row->set_id(row.id);
                proto_row->set_username(row.username);
                proto_row->set_email(row.email);

                cursor->cursor_advance();
            }
            delete cursor;
            response->set_status(bpsndb::CommandResponse::SUCCESS);
            response->set_message("Select Executed");
        }  else {
            // INSERT, UPDATE, DELETE
            ExecuteResult result = statement.execute_statement(table);
            if (result == EXECUTE_SUCCESS) {
                cout<<"In the Primary DB"<<endl;
                response->set_status(bpsndb::CommandResponse::SUCCESS);
                response->set_message("Executed successfully");
            } else {
                response->set_status(bpsndb::CommandResponse::FAILURE);
                response->set_message("Execution failed or key error");
            }
        }

        return grpc::Status::OK;
    }
};

void RunServer(Table* table, string port) {
    string server_address("0.0.0.0:" + port);
    DatabaseEngineImpl service(table);

    grpc::ServerBuilder builder;

    builder.SetMaxReceiveMessageSize(1024 * 1024 * 50); 
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<grpc::Server> server(builder.BuildAndStart());
    cout << "C++ Engine listening on " << server_address << endl;
    server->Wait();
}

// the input arguements are always argc = x + 1, 1 becuase of the run command of the cpp/c file and then the input arguement array
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Must supply a database filename and port\n");
        exit(EXIT_FAILURE);
    }

    char* filename = argv[1];
    string port = argv[2];
    Table* table = new Table(filename);

    RunServer(table, port);

    // InputBuffer* inputBuffer = new InputBuffer();

    // Continously reading the db input
    // while(1) {
    //     inputBuffer->print_prompt();
    //     inputBuffer->read_input();

    //     Statement statement;

    //     if (inputBuffer->buffer[0] == '.') {
    //         switch (inputBuffer->do_meta_command(table)) {
    //             case (META_COMMAND_SUCCESS):
    //                 continue;
    //             case (META_COMMAND_UNRECOGNIZED_COMMAND):
    //                 printf("Unrecognized command '%s'\n", inputBuffer->buffer);
    //             continue;
    //         }
    //     }

    //     switch (statement.prepare_statement(inputBuffer->buffer)) {
    //         case (PREPARE_SUCCESS):
    //             break;
    //         case (PREPARE_SYNTAX_ERROR):
    //             printf("Syntax error. Could not parse statement.\n");
    //             continue;
    //         case (PREPARE_UNRECOGNIZED_STATEMENT):
    //             printf("Unrecognized keyword at start of '%s'.\n",inputBuffer->buffer);
    //             continue;
    //         case (PREPARE_STRING_TOO_LONG):
    //             printf("String is too long.\n");
    //             continue;
    //         case (PREPARE_NEGATIVE_ID):
    //             printf("ID must be positive.\n");
    //             continue;
    //     }

    //     switch (statement.execute_statement(table)) {
    //         case (EXECUTE_SUCCESS):
    //             printf("Executed.\n");
    //             break;
    //         case (EXECUTE_DUPLICATE_KEY):
    //             printf("Error: Duplicate key.\n");
    //             break;
    //         case (EXECUTE_TABLE_FULL):
    //             printf("Error: Table full.\n");
    //             break;
    //         case (EXECUTE_KEY_NOT_FOUND):
    //             printf("Can't find the key to update.\n");
    //             break;
    //     }
    // }

    // Free up the heap
    delete table;

    // To tell the shell that the program executed correctly, or else it would be anything other than 0.
    // Weird right? => How many things went wrong = 0
    return 0;
}
