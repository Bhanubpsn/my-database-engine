#include <cstdint>
#include "table.h"
#ifndef CURSOR_H
#define CURSOR_H

class Cursor {
public:
    Table* table;
    uint32_t row_num;
    bool end_of_table;  // Indicates a position one past the last element of the table

    void table_start(Table* table) {
        this->table = table;
        this->row_num = 0;
        this->end_of_table = (table->num_rows == 0);
    }

    void table_end(Table* table) {
        this->table = table;
        this->row_num = table->num_rows;
        this->end_of_table = true;
    }

    void cursor_advance() {
        this->row_num += 1;
        if (this->row_num >= this->table->num_rows) {
            this->end_of_table = true;
        }
    }

};

#endif