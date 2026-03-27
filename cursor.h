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
        table_find(table, 0);
        void* root_node = table->pager->get_page(table->root_page_num);
        InternalNode rootNode;                  // Making it static so ne memory leaks occurs
        rootNode.node = (uint8_t*)root_node;
        uint32_t num_cells = *rootNode.leaf_node_num_cells();
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
            // Advance to next leaf node 
            uint32_t next_page_num = *leafNode.leaf_node_next_leaf();
            if (next_page_num == 0) {
                // This was rightmost leaf i.e. no siblings left
                this->end_of_table = true;
            } else {
                this->page_num = next_page_num;
                this->cell_num = 0;
            }
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

    uint32_t internal_node_find_child(InternalNode node, uint32_t key) {
        // Return the index of the child which should contain the given key.
        uint32_t num_keys = *node.internal_node_num_keys();

        /* Binary search to find index of child to search */
        uint32_t min_index = 0;
        uint32_t max_index = num_keys; /* there is one more child than key */

        while (min_index != max_index) {
            uint32_t index = (min_index + max_index) / 2;
            uint32_t key_to_right = *node.internal_node_key(index);
            if (key_to_right >= key) {
                max_index = index;
            } else {
                min_index = index + 1;
            }
        }
        return min_index;
    }

    void internal_node_find(uint32_t page_num, uint32_t key) {
        void* node = this->table->pager->get_page(page_num);
        InternalNode internalNode;
        internalNode.node = (uint8_t*)node;

        uint32_t child_index = internal_node_find_child(internalNode, key);
        uint32_t child_num = *internalNode.internal_node_child(child_index);

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

        if (rootNode.get_node_type() == NODE_INTERNAL) {
            rightChild.initialize_internal_node();
            leftChild.initialize_internal_node();
        }

        /* Left child has data copied from old root */

        memcpy(left_child, root, PAGE_SIZE);
        leftChild.set_node_root(false);

        if (leftChild.get_node_type() == NODE_INTERNAL) {
            InternalNode childNode;
            for (int i = 0; i < *leftChild.internal_node_num_keys(); i++) {
                childNode.node = (uint8_t*)this->table->pager->get_page(*leftChild.internal_node_child(i));
                *childNode.node_parent() = left_child_page_num;
            }
            childNode.node = (uint8_t*)this->table->pager->get_page(*leftChild.internal_node_right_child());
            *childNode.node_parent() = left_child_page_num;
        }

        /* Root node is a new internal node with one key and two children */

        rootNode.initialize_internal_node();
        rootNode.set_node_root(true);

        *rootNode.internal_node_num_keys() = 1;
        *rootNode.internal_node_child(0) = left_child_page_num;
        
        uint32_t left_child_max_key = leftChild.get_node_max_key(this->table->pager);
        *rootNode.internal_node_key(0) = left_child_max_key;
        *rootNode.internal_node_right_child() = right_child_page_num;
        *leftChild.node_parent() = this->table->root_page_num;
        *rightChild.node_parent() = this->table->root_page_num;
    }

    void update_internal_node_key(InternalNode node, uint32_t old_key, uint32_t new_key) {
        uint32_t old_child_index = internal_node_find_child(node, old_key);
        *node.internal_node_key(old_child_index) = new_key;
    }

    void internal_node_split_and_insert(uint32_t parent_page_num, uint32_t child_page_num) {
        uint32_t old_page_num = parent_page_num;
        void* old_node = this->table->pager->get_page(parent_page_num);
        InternalNode oldNode;
        oldNode.node = (uint8_t*)old_node;
        uint32_t old_max = oldNode.get_node_max_key(this->table->pager);

        void* child = this->table->pager->get_page(child_page_num); 
        InternalNode childNode;
        childNode.node = (uint8_t*)child;
        uint32_t child_max = childNode.get_node_max_key(this->table->pager);

        uint32_t new_page_num = this->table->pager->get_unused_page_num();

        // Declaring a flag before updating pointers which
        // records whether this operation involves splitting the root -
        // if it does, we will insert our newly created node during
        // the step where the table's new root is created. If it does
        // not, we have to insert the newly created node into its parent
        // after the old node's keys have been transferred over. We are not
        // able to do this if the newly created node's parent is not a newly
        // initialized root node, because in that case its parent may have existing
        // keys aside from our old node which we are splitting. If that is true, we
        // need to find a place for our newly created node in its parent, and we
        // cannot insert it at the correct index if it does not yet have any keys
        
        uint32_t splitting_root = oldNode.is_node_root();

        void* parent;
        InternalNode parentNode;
        void* new_node;
        InternalNode newNode;
        if (splitting_root) {
            create_new_root(new_page_num);
            parent = this->table->pager->get_page(this->table->root_page_num);
            /*
                If we are splitting the root, we need to update old_node to point
                to the new root's left child, new_page_num will already point to
                the new root's right child
            */
            parentNode.node = (uint8_t*)parent;
            old_page_num = *parentNode.internal_node_child(0);
            old_node = this->table->pager->get_page(old_page_num);
        } else {
            parent = this->table->pager->get_page(*oldNode.node_parent());
            parentNode.node = (uint8_t*)parent;
            new_node = this->table->pager->get_page(new_page_num);
            newNode.node = (uint8_t*)new_node;
            newNode.initialize_internal_node();
        }
        
        uint32_t* old_num_keys = oldNode.internal_node_num_keys();

        uint32_t cur_page_num = *oldNode.internal_node_right_child();
        void* cur = this->table->pager->get_page(cur_page_num);
        InternalNode curr;
        curr.node = (uint8_t*)cur;

        // First put right child into new node and set right child of old node to invalid page number

        internal_node_insert(new_page_num, cur_page_num);
        *curr.node_parent() = new_page_num;
        *oldNode.internal_node_right_child() = INVALID_PAGE_NUM;

        // For each key until you get to the middle key, move the key and the child to the new node

        for (int i = INTERNAL_NODE_MAX_CELLS - 1; i > INTERNAL_NODE_MAX_CELLS / 2; i--) {
            cur_page_num = *oldNode.internal_node_child(i);
            cur = this->table->pager->get_page(cur_page_num);

            internal_node_insert(new_page_num, cur_page_num);
            *curr.node_parent() = new_page_num;

            (*old_num_keys)--;
        }

        /*
            Set child before middle key, which is now the highest key, to be node's right child,
            and decrement number of keys
        */
        *oldNode.internal_node_right_child() = *oldNode.internal_node_child(*old_num_keys - 1);
        (*old_num_keys)--;

        /*
            Determine which of the two nodes after the split should contain the child to be inserted,
            and insert the child
        */
        uint32_t max_after_split = oldNode.get_node_max_key(this->table->pager);

        uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;

        internal_node_insert(destination_page_num, child_page_num);
        *childNode.node_parent() = destination_page_num;

        update_internal_node_key(parentNode, old_max, oldNode.get_node_max_key(this->table->pager));

        if (!splitting_root) {
            internal_node_insert(*oldNode.node_parent(),new_page_num);
            *newNode.node_parent() = *oldNode.node_parent();
        }
    }


    void internal_node_insert(uint32_t parent_page_num, uint32_t child_page_num) {
        // Add a new child/key pair to parent that corresponds to child

        void* parent = this->table->pager->get_page(parent_page_num);
        InternalNode parentNode;
        parentNode.node = (uint8_t*)parent;

        void* child = this->table->pager->get_page(child_page_num);
        InternalNode childNode;
        childNode.node = (uint8_t*) child;

        uint32_t child_max_key = childNode.get_node_max_key(this->table->pager);
        uint32_t index = internal_node_find_child(parentNode, child_max_key);

        uint32_t original_num_keys = *parentNode.internal_node_num_keys();
        // *parentNode.internal_node_num_keys() = original_num_keys + 1;

        if (original_num_keys >= INTERNAL_NODE_MAX_CELLS) {
            internal_node_split_and_insert(parent_page_num, child_page_num);
            return;
        }

        uint32_t right_child_page_num = *parentNode.internal_node_right_child();
        /*
            An internal node with a right child of INVALID_PAGE_NUM is empty
        */
        if (right_child_page_num == INVALID_PAGE_NUM) {
            *parentNode.internal_node_right_child() = child_page_num;
            return;
        }

        void* right_child = this->table->pager->get_page(right_child_page_num);
        InternalNode rightChildNode;
        rightChildNode.node = (uint8_t*)right_child;

        /*
            If we are already at the max number of cells for a node, we cannot increment
            before splitting. Incrementing without inserting a new key/child pair
            and immediately calling internal_node_split_and_insert has the effect
            of creating a new key at (max_cells + 1) with an uninitialized value
        */
        *parentNode.internal_node_num_keys() = original_num_keys + 1;


        if (child_max_key > rightChildNode.get_node_max_key(this->table->pager)) {
            /* Replace right child */
            *parentNode.internal_node_child(original_num_keys) = right_child_page_num;
            *parentNode.internal_node_key(original_num_keys) = rightChildNode.get_node_max_key(this->table->pager);
            *parentNode.internal_node_right_child() = child_page_num;
        } else {
            /* Make room for the new cell */
            for (uint32_t i = original_num_keys; i > index; i--) {
                void* destination = parentNode.internal_node_cell(i);
                void* source = parentNode.internal_node_cell(i - 1);
                memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
            }
            *parentNode.internal_node_child(index) = child_page_num;
            *parentNode.internal_node_key(index) = child_max_key;
        }
    }

    void leaf_node_split_and_insert(uint32_t key, Row value) {
        /*
            Create a new node and move half the cells over.
            Insert the new value in one of the two nodes.
            Update parent or create a new parent.
        */

        void* old_node = this->table->pager->get_page(this->page_num);
        LeafNode oldNode;
        oldNode.node = (uint8_t*)old_node;

        InternalNode oldInternalNode;
        oldInternalNode.node = (uint8_t*)old_node;
        uint32_t old_max = oldInternalNode.get_node_max_key();

        uint32_t new_page_num = this->table->pager->get_unused_page_num();
        void* new_node = this->table->pager->get_page(new_page_num);
        
        LeafNode newNode;
        newNode.node = (uint8_t*)new_node;
        newNode.initialize_leaf_node(false);

        *newNode.node_parent() = *oldNode.node_parent();

        *newNode.leaf_node_next_leaf() = *oldNode.leaf_node_next_leaf();
        *oldNode.leaf_node_next_leaf() = new_page_num;

        /*
            All existing keys plus new key should be divided
            evenly between old (left) and new (right) nodes.
            Starting from the right, move each key to correct position.
        */

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
                value.serialize_row(destinationNode.leaf_node_value(index_within_node));
                *destinationNode.leaf_node_key(index_within_node) = key;
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
            uint32_t parent_page_num = *oldInternalNode.node_parent();
            uint32_t new_max = oldInternalNode.get_node_max_key();
            void* parent = this->table->pager->get_page(parent_page_num);
            InternalNode parentNode;
            parentNode.node = (uint8_t*)parent;

            update_internal_node_key(parentNode ,old_max, new_max);
            internal_node_insert(parent_page_num, new_page_num);
            return;
        }
        
    };

};

#endif
