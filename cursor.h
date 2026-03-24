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
            internal_node_find(root_page_num, key);
            return;
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
            // printf("Need to implement splitting a leaf node.\n");
            // exit(EXIT_FAILURE);
            leaf_node_split_and_insert(key, value);
            return;
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

    void internal_node_find(uint32_t page_num, uint32_t key) {
        void* node = this->table->pager->get_page(page_num);
        InternalNode internalNode;
        internalNode.node = (uint8_t*)node;
        uint32_t num_keys = *internalNode.internal_node_num_keys();

        /* Binary search to find index of child to search */
        uint32_t min_index = 0;
        uint32_t max_index = num_keys; /* there is one more child than key */

        while (min_index != max_index) {
            uint32_t index = (min_index + max_index) / 2;
            uint32_t key_to_right = *internalNode.internal_node_key(index);
            if (key_to_right >= key) {
                max_index = index;
            } else {
                min_index = index + 1;
            }
        }
        uint32_t child_num = *internalNode.internal_node_child(min_index);
        void* child = this->table->pager->get_page(child_num);
        InternalNode childNode;
        childNode.node = (uint8_t*)child;
        switch (childNode.get_node_type()) {
            case NODE_LEAF:
                leaf_node_find(child_num, key);
                return;
            case NODE_INTERNAL:
                internal_node_find(child_num, key);
                return;
        }
    }

    void create_new_root(uint32_t right_child_page_num) {
        /*
            Handle splitting the root.
            Old root copied to new page, becomes left child.
            Address of right child passed in.
            Re-initialize root page to contain the new root node.
            New root node points to two children.
        */

        void* root = this->table->pager->get_page(this->table->root_page_num);
        void* right_child = this->table->pager->get_page(right_child_page_num);
        uint32_t left_child_page_num = this->table->pager->get_unused_page_num();
        void* left_child = this->table->pager->get_page(left_child_page_num);

        InternalNode rootNode;
        rootNode.node = (uint8_t*)root;
        InternalNode leftChild;
        leftChild.node = (uint8_t*)left_child;
        InternalNode rightChild;
        rightChild.node = (uint8_t*)right_child;

        /* Left child has data copied from old root */

        memcpy(left_child, root, PAGE_SIZE);
        leftChild.set_node_root(false);

        /* Root node is a new internal node with one key and two children */

        rootNode.initialize_internal_node();
        rootNode.set_node_root(true);

        *rootNode.internal_node_num_keys() = 1;
        *rootNode.internal_node_child(0) = left_child_page_num;
        
        uint32_t left_child_max_key = leftChild.get_node_max_key();
        *rootNode.internal_node_key(0) = left_child_max_key;
        *rootNode.internal_node_right_child() = right_child_page_num;
    }

    void leaf_node_split_and_insert(uint32_t key, Row value) {
        /*
            Create a new node and move half the cells over.
            Insert the new value in one of the two nodes.
            Update parent or create a new parent.
        */

        void* old_node = this->table->pager->get_page(this->page_num);
        uint32_t new_page_num = this->table->pager->get_unused_page_num();
        void* new_node = this->table->pager->get_page(new_page_num);
        LeafNode newNode;
        newNode.node = (uint8_t*)new_node;
        newNode.initialize_leaf_node(false);

        /*
            All existing keys plus new key should be divided
            evenly between old (left) and new (right) nodes.
            Starting from the right, move each key to correct position.
        */

        LeafNode oldNode;
        oldNode.node = (uint8_t*)old_node;

        for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
            void* destination_node;
            if (i >= LEAF_NODE_LEFT_SPLIT_COUNT) {
                destination_node = new_node;
            } else {
                destination_node = old_node;
            }
            LeafNode destinationNode;
            destinationNode.node = (uint8_t*)destination_node;
            uint32_t index_within_node = (i >= LEAF_NODE_LEFT_SPLIT_COUNT) ? i - LEAF_NODE_LEFT_SPLIT_COUNT : i;
            void* destination = destinationNode.leaf_node_cell(index_within_node);

            if (i == this->cell_num) {
                value.serialize_row(destination);
            } else if (i > this->cell_num) {
                memcpy(destination, oldNode.leaf_node_cell(i - 1), LEAF_NODE_CELL_SIZE);
            } else {
                memcpy(destination, oldNode.leaf_node_cell(i), LEAF_NODE_CELL_SIZE);
            }
        }

        /* Update cell count on both leaf nodes */

        *(oldNode.leaf_node_num_cells()) = LEAF_NODE_LEFT_SPLIT_COUNT;
        *(newNode.leaf_node_num_cells()) = LEAF_NODE_RIGHT_SPLIT_COUNT;

        if(oldNode.is_node_root()) {
            return create_new_root(new_page_num);
        } else {
            printf("Need to implement updating parent after split\n");
            exit(EXIT_FAILURE);
        }
        
    };

};

#endif
