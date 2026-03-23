#include <cstdint>
#include "table.h"
#include "pager.h"
#include "b-tree.h"
#include "row.h"
#ifndef CURSOR_H
#define CURSOR_H

class Cursor {
public:
    Table* table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table;  // Indicates a position one past the last element of the table

    void table_start(Table* table) {
        this->table = table;
        this->page_num = table->root_page_num;
        this->cell_num = 0;

        void* root_node = table->pager->get_page(table->root_page_num);
        LeafNode leafNode;                  // Making it static so ne memory leaks occurs
        leafNode.node = (uint8_t*)root_node;
        uint32_t num_cells = *leafNode.leaf_node_num_cells();
        this->end_of_table = (num_cells == 0);
    }

    void table_end(Table* table) {
        this->table = table;
        this->page_num = table->root_page_num;

        void* root_node = table->pager->get_page(table->root_page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)root_node;
        uint32_t num_cells = *leafNode.leaf_node_num_cells();
        this->cell_num = num_cells;
        this->end_of_table = true;
    }

    void cursor_advance() {
        uint32_t page_num = this->page_num;

        void* node = this->table->pager->get_page(page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)node;
        this->cell_num += 1;
        if (this->cell_num >= (*leafNode.leaf_node_num_cells())) {
            this->end_of_table = true;
        }
    }

    void leaf_node_insert(uint32_t key, Row value) {
        void* node = this->table->pager->get_page(this->page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)node;
        uint32_t num_cells = *leafNode.leaf_node_num_cells();
        if (num_cells >= LEAF_NODE_MAX_CELLS) {
            // Node full
            printf("Need to implement splitting a leaf node.\n");
            exit(EXIT_FAILURE);
        }

        if (this->cell_num < num_cells) {
            // Make room for new cell
            for (uint32_t i = num_cells; i > this->cell_num; i--) {
                memcpy(leafNode.leaf_node_cell(i), leafNode.leaf_node_cell(i - 1), LEAF_NODE_CELL_SIZE);
            }
        }

        *(leafNode.leaf_node_num_cells()) += 1;
        *(leafNode.leaf_node_key(this->cell_num)) = key;
        value.serialize_row(leafNode.leaf_node_value(this->cell_num));
    }

};

#endif
