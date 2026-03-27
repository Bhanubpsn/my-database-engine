#include <iostream>
#include "types.h"
#include "row.h"
#include "pager.h"
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
const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET = LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE;

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
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;
const uint32_t LEAF_NODE_MIN_CELLS = LEAF_NODE_MAX_CELLS / 2;

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

    // Helper for internal use to get a raw address
    uint8_t* leaf_node_cell_ptr(uint32_t cell_num) {
        return node + LEAF_NODE_HEADER_SIZE + (cell_num * LEAF_NODE_CELL_SIZE);
    }

    uint32_t *leaf_node_cell(uint32_t cell_num) {
        return (uint32_t *)(node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE);
    }

    uint32_t *leaf_node_key(uint32_t cell_num) {
        return (uint32_t *)leaf_node_cell_ptr(cell_num);
    }

    void* leaf_node_value(uint32_t cell_num) {
        return leaf_node_cell_ptr(cell_num) + LEAF_NODE_KEY_SIZE;
    }

    void set_node_type(NodeType type) {
        uint8_t value = type;
        *((uint8_t*)(node + NODE_TYPE_OFFSET)) = value;
    }

    void initialize_leaf_node(bool is_root) {
        *leaf_node_num_cells() = 0;
        set_node_type(NODE_LEAF);
        set_node_root(false);
        *(node + IS_ROOT_OFFSET) = is_root ? 1 : 0;
        uint32_t *parent = (uint32_t *)(node + PARENT_POINTER_OFFSET);
        *parent = 0;
        *leaf_node_next_leaf() = 0;  // 0 represents no sibling
    }

    uint32_t get_node_max_key() {
        uint32_t num = 0;
        num = *leaf_node_num_cells();
        return (num == 0) ? 0 : *leaf_node_key(num - 1);
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

    bool is_node_root() {
        uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
        return (bool)value;
    }

    void set_node_root(bool is_root) {
        uint8_t value = is_root;
        *((uint8_t*)(node + IS_ROOT_OFFSET)) = value;
    }

    uint32_t* leaf_node_next_leaf() {
        return (uint32_t*)(node + LEAF_NODE_NEXT_LEAF_OFFSET);
    }

    uint32_t* node_parent() {
        return (uint32_t*)(node + PARENT_POINTER_OFFSET);
    }
};

/*
    Internal Node Header Layout
*/

const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET = INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;
const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE;

/*
    Internal Node Body Layout
*/

const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CELL_SIZE = INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;
const uint32_t INTERNAL_NODE_MAX_CELLS = 3;
const uint32_t INTERNAL_NODE_MIN_KEYS = INTERNAL_NODE_MAX_CELLS / 2;

/*
    Internal Node Byte Map =>
    Byte 0 -> node_type
    Byte 1 -> is_root
    Byte 2-5 -> parent pointer
    Byte 6-9 -> num_keys
    Byte 10-13 -> right child pointer
    Byte 14-17 -> child pointer
    Byte 18-21 -> Key
    ...
    ...
    Byte 4086-4089 -> child point 
    Byte 4090-4093 -> key
    Byte 4094-4095 -> Wasted space

*/

class InternalNode : public LeafNode {
public:

    uint32_t* internal_node_num_keys() {
        return (uint32_t*)(node + INTERNAL_NODE_NUM_KEYS_OFFSET);
    }

    uint32_t* internal_node_right_child() {
        return (uint32_t*)(node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
    }

    uint32_t* internal_node_cell(uint32_t cell_num) {
        return (uint32_t*)(node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE);
    }

    // Helper for internal use
    uint8_t* internal_node_cell_ptr(uint32_t cell_num) {
        return node + INTERNAL_NODE_HEADER_SIZE + (cell_num * INTERNAL_NODE_CELL_SIZE);
    }

    uint32_t* internal_node_child(uint32_t child_num) {
        uint32_t num_keys = *internal_node_num_keys();
        if (child_num > num_keys) {
            printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
            exit(EXIT_FAILURE);
        } else if (child_num == num_keys) {
            uint32_t* right_child = internal_node_right_child();
            if (*right_child == INVALID_PAGE_NUM) {
                printf("Tried to access right child of node, but was invalid page\n");
                exit(EXIT_FAILURE);
            }
            return right_child;
        } else {
            uint32_t* child = (uint32_t*)internal_node_cell_ptr(child_num);
            if (*child == INVALID_PAGE_NUM) {
                printf("Tried to access child %d of node, but was invalid page\n", child_num);
                exit(EXIT_FAILURE);
            }
            return child;
        }
    }

    uint32_t* internal_node_key(uint32_t key_num) {
        // Explicitly move 4 bytes (the size of a child pointer) past the cell start
        return (uint32_t*)(internal_node_cell_ptr(key_num) + INTERNAL_NODE_CHILD_SIZE);
    }

    uint32_t get_node_max_key(Pager* pager) {
        if (get_node_type() == NODE_LEAF) {
            return *leaf_node_key(*leaf_node_num_cells() - 1);
        }
        void* right_child = pager->get_page(*internal_node_right_child());
        InternalNode rightChild;
        rightChild.node = (uint8_t*)right_child;
        return rightChild.get_node_max_key(pager);
    }

    void initialize_internal_node() {
        set_node_type(NODE_INTERNAL);
        set_node_root(false);
        *internal_node_num_keys() = 0;
        /*
            Necessary because the root page number is 0; by not initializing an internal 
            node's right child to an invalid page number when initializing the node, we may
            end up with 0 as the node's right child, which makes the node a parent of the root
        */
        *internal_node_right_child() = INVALID_PAGE_NUM;
    }

    void indent(uint32_t level) {
        for (uint32_t i = 0; i < level; i++) {
            printf("  ");
        }
    }

    void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level) {
        void* n = pager->get_page(page_num);
        uint32_t num_keys, child;
        InternalNode node;
        node.node = (uint8_t*)n;

        switch (node.get_node_type()) {
            case (NODE_LEAF):
                num_keys = *node.leaf_node_num_cells();
                indent(indentation_level);
                printf("- leaf (size %d)\n", num_keys);
                for (uint32_t i = 0; i < num_keys; i++) {
                    indent(indentation_level + 1);
                    printf("- %d\n", *node.leaf_node_key(i));
                }
                break;
            case (NODE_INTERNAL):
                num_keys = *node.internal_node_num_keys();
                indent(indentation_level);
                printf("- internal (size %d)\n", num_keys);
                if (num_keys > 0) {
                    for (uint32_t i = 0; i < num_keys; i++) {
                    child = *internal_node_child(i);
                    print_tree(pager, child, indentation_level + 1);

                    indent(indentation_level + 1);
                    printf("- key %d\n", *internal_node_key(i));
                }
                    child = *internal_node_right_child();
                    print_tree(pager, child, indentation_level + 1);
                }
                break;
        }
    }
};

#endif
