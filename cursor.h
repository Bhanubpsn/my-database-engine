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

    /*
        Return the position of the given key.
        If the key is not present, return the position
        where it should be inserted
    */
    void table_find(Table* table, uint32_t key) {
        this->table = table;
        uint32_t root_page_num = table->root_page_num;
        void* root_node = table->pager->get_page(root_page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)root_node;

        if (leafNode.get_node_type() == NODE_LEAF) {
            leaf_node_find(root_page_num, key);
            return;
        } else {
            printf("Need to implement searching an internal node\n");
            exit(EXIT_FAILURE);
        }
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

    void leaf_node_find(uint32_t page_num, uint32_t key) {
        void* node = this->table->pager->get_page(page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)node;
        uint32_t num_cells = *leafNode.leaf_node_num_cells();

        this->page_num = page_num;

        // Binary search
        uint32_t min_index = 0;
        uint32_t one_past_max_index = num_cells;
        while (one_past_max_index != min_index) {
            uint32_t index = (min_index + one_past_max_index) / 2;
            uint32_t key_at_index = *leafNode.leaf_node_key(index);
            if (key == key_at_index) {
                this->cell_num = index;
                return;
            }
            if (key < key_at_index) {
                one_past_max_index = index;
            } else {
                min_index = index + 1;
            }
        }
        this->cell_num = min_index;
        return;
    }

};

#endif
