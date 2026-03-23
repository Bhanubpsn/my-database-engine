#include <iostream>
#include "types.h"
#include "row.h"
#ifndef BTREE_H
#define BTREE_H

// Each node will store one page and at the header of the page it will store some metadata, so we need
// to have some offsets in the pages for that metadata

/*
Common Header Layout
*/

const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
const uint32_t NODE_TYPE_OFFSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
const uint8_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

/*
Leaf Node Header Layout
*/

const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;

/*
Leaf Node Body Layout
*/

const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
const uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

/*
    Node (A single page) Byte Map =>

    Byte 0 -> node_type
    Byte 1 -> is_root
    Byte 2-5 -> parent_pointer
    Byte 6-9 -> num_cells
    Byte 10-13 -> key 0, Byte 14-306 -> value0 (A whole Row)
    ...
    ...
    Till Byte 3871, Key Value(Row) pairs
    Rest Byte 3872-4095 Wasted Space
*/

class LeafNode {
public:
    // Keeping it Byte sized, if any addition to unit32_t* will result in Byte*4 addition
    uint8_t *node;

    uint32_t *leaf_node_num_cells() {
        // Offset is 6 bytes. node + 6 moves 6 bytes forward. And not 6*4 24 Bytes
        return (uint32_t *)(node + LEAF_NODE_NUM_CELLS_OFFSET);
    }

    uint32_t *leaf_node_cell(uint32_t cell_num) {
        return (uint32_t *)(node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE);
    }

    uint32_t *leaf_node_key(uint32_t cell_num) {
        return leaf_node_cell(cell_num);
    }

    uint32_t *leaf_node_value(uint32_t cell_num) {
        return leaf_node_cell(cell_num) + LEAF_NODE_KEY_SIZE;
    }

    void set_node_type(NodeType type) {
        uint8_t value = type;
        *((uint8_t*)(node + NODE_TYPE_OFFSET)) = value;
    }

    void initialize_leaf_node(bool is_root) {
        *leaf_node_num_cells() = 0;
        set_node_type(NODE_LEAF);
        *(node + IS_ROOT_OFFSET) = is_root ? 1 : 0;
        uint32_t *parent = (uint32_t *)(node + PARENT_POINTER_OFFSET);
        *parent = 0;
    }

    NodeType get_node_type() {
        uint8_t value = *((uint8_t*)(node + NODE_TYPE_OFFSET));
        return (NodeType)value;
    }

    void print_leaf_node(){
        uint32_t num_cells = *leaf_node_num_cells();
        printf("leaf (size %d)\n", num_cells);
        for (uint32_t i = 0; i < num_cells; i++) {
            uint32_t key = *leaf_node_key(i);
            printf("  - %d : %d\n", i, key);
        }
    }
};

#endif
