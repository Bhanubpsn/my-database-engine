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
    
    void leaf_node_delete(uint32_t key) {
        void* node = this->table->pager->get_page(this->page_num);
        LeafNode leafNode;
        leafNode.node = (uint8_t*)node;
        uint32_t num_cells = *leafNode.leaf_node_num_cells();

        // Logical Delete (Shifting cells left)
        for (uint32_t i = this->cell_num; i < num_cells - 1; i++) {
            memcpy(leafNode.leaf_node_cell(i), leafNode.leaf_node_cell(i + 1), LEAF_NODE_CELL_SIZE);
        }
        *(leafNode.leaf_node_num_cells()) -= 1;

        // Underflow
        if (!leafNode.is_node_root() && *leafNode.leaf_node_num_cells() < LEAF_NODE_MIN_CELLS) {
            leaf_node_rebalance(this->page_num);
        } else if (leafNode.is_node_root() && *leafNode.leaf_node_num_cells() == 0) {
            // Table became completely empty
            leafNode.initialize_leaf_node(true);
        }
    }

    void leaf_node_rebalance(uint32_t page_num) {
        void* node_ptr = this->table->pager->get_page(page_num);
        InternalNode node;
        node.node = (uint8_t*)node_ptr;
        
        uint32_t parent_page_num = *node.node_parent();
        void* parent_ptr = this->table->pager->get_page(parent_page_num);
        InternalNode parent;
        parent.node = (uint8_t*)parent_ptr;

        // Find our index in the parent to find siblings
        uint32_t node_max_key = node.get_node_max_key(this->table->pager);
        uint32_t own_index = internal_node_find_child(parent, node_max_key);

        // Try to borrow from Left Sibling first
        if (own_index > 0) {
            uint32_t left_sibling_page = *parent.internal_node_child(own_index - 1);
            if (can_borrow_from(left_sibling_page)) {
                return borrow_from_left(page_num, left_sibling_page, own_index - 1);
            }
        }

        // If we can't borrow, we MUST merge
        if (own_index > 0) {
            merge_nodes(parent_page_num, own_index - 1, own_index); // Merge with left
        } else {
            merge_nodes(parent_page_num, own_index, own_index + 1); // Merge with right
        }
    }

    bool can_borrow_from(uint32_t page_num) {
        void* node_ptr = this->table->pager->get_page(page_num);
        LeafNode node;
        node.node = (uint8_t*)node_ptr;
        
        // Check if its a leaf or internal and compare to MIN constants
        if (node.get_node_type() == NODE_LEAF) {
            return *node.leaf_node_num_cells() > LEAF_NODE_MIN_CELLS;
        } else {
            InternalNode internal;
            internal.node = (uint8_t*)node_ptr;
            return *internal.internal_node_num_keys() > INTERNAL_NODE_MIN_KEYS;
        }
    }

    void borrow_from_left(uint32_t receiver_page_num, uint32_t donor_page_num, uint32_t parent_divider_index) {
        LeafNode receiver;
        receiver.node = (uint8_t*)this->table->pager->get_page(receiver_page_num);
        LeafNode donor;
        donor.node = (uint8_t*)this->table->pager->get_page(donor_page_num);
        
        uint32_t receiver_count = *receiver.leaf_node_num_cells();
        uint32_t donor_count = *donor.leaf_node_num_cells();

        // Make a hole at index 0 in the receiver
        for (uint32_t i = receiver_count; i > 0; i--) {
            memcpy(receiver.leaf_node_cell(i), receiver.leaf_node_cell(i - 1), LEAF_NODE_CELL_SIZE);
        }

        // Copying donors last cell to receiver's index 0
        memcpy(receiver.leaf_node_cell(0), donor.leaf_node_cell(donor_count - 1), LEAF_NODE_CELL_SIZE);
        
        *receiver.leaf_node_num_cells() = receiver_count + 1;
        *donor.leaf_node_num_cells() = donor_count - 1;

        InternalNode parent;
        parent.node = (uint8_t*)this->table->pager->get_page(*receiver.node_parent());
        *parent.internal_node_key(parent_divider_index) = donor.get_node_max_key();
    }

    void merge_nodes(uint32_t parent_page, uint32_t left_index, uint32_t right_index) {
        InternalNode parent;
        parent.node = (uint8_t*)this->table->pager->get_page(parent_page);
        
        uint32_t left_page_num = *parent.internal_node_child(left_index);
        uint32_t right_page_num = *parent.internal_node_child(right_index);
        
        LeafNode left;
        left.node = (uint8_t*)this->table->pager->get_page(left_page_num);
        LeafNode right;
        right.node = (uint8_t*)this->table->pager->get_page(right_page_num);

        // all cells from right to left
        uint32_t left_count = *left.leaf_node_num_cells();
        uint32_t right_count = *right.leaf_node_num_cells();

        for (uint32_t i = 0; i < right_count; i++) {
            memcpy(left.leaf_node_cell(left_count + i), right.leaf_node_cell(i), LEAF_NODE_CELL_SIZE);
        }
        *left.leaf_node_num_cells() += right_count;
        *left.leaf_node_next_leaf() = *right.leaf_node_next_leaf();

        // Remove the divider from the parent
        remove_internal_node_key(parent, right_index);
    }

    void remove_internal_node_key(InternalNode parent, uint32_t index) {
        uint32_t num_keys = *parent.internal_node_num_keys();

        // If we are removing the rightmost child, we need to promote a new right child
        if (index == num_keys) {
            *parent.internal_node_right_child() = *parent.internal_node_child(num_keys - 1);
        } else {
            // Shift keys and pointers left
            for (uint32_t i = index; i < num_keys - 1; i++) {
                memcpy(parent.internal_node_cell(i), parent.internal_node_cell(i + 1), INTERNAL_NODE_CELL_SIZE);
            }
        }
        
        *parent.internal_node_num_keys() = num_keys - 1;

        // Check if parent now underflows 
        if (!parent.is_node_root() && *parent.internal_node_num_keys() < INTERNAL_NODE_MIN_KEYS) {
            internal_node_rebalance(*parent.node_parent()); 
        }
    }

    void internal_node_rebalance(uint32_t page_num) {
        void* node_ptr = this->table->pager->get_page(page_num);
        InternalNode node;
        node.node = (uint8_t*)node_ptr;

        if (node.is_node_root()) {
            // If the root has only 1 child left, 
            // that child becomes the new root (reducing tree height)
            if (*node.internal_node_num_keys() == 0) {
                uint32_t only_child_page = *node.internal_node_child(0);
                this->table->root_page_num = only_child_page;
                
                void* child_ptr = this->table->pager->get_page(only_child_page);
                InternalNode childNode;
                childNode.node = (uint8_t*)child_ptr;
                childNode.set_node_root(true);
                *childNode.node_parent() = 0; // Root has no parent
            }
            return;
        }

        uint32_t parent_page_num = *node.node_parent();
        InternalNode parent;
        parent.node = (uint8_t*)this->table->pager->get_page(parent_page_num);
        
        uint32_t node_max_key = node.get_node_max_key(this->table->pager);
        uint32_t own_index = internal_node_find_child(parent, node_max_key);

        // Try to borrow from Left Sibling
        if (own_index > 0) {
            uint32_t left_sibling_page = *parent.internal_node_child(own_index - 1);
            if (can_borrow_from(left_sibling_page)) {
                return borrow_internal_from_left(page_num, left_sibling_page, own_index - 1);
            }
        }

        // Try to borrow from Right Sibling
        uint32_t num_parent_keys = *parent.internal_node_num_keys();
        if (own_index < num_parent_keys) {
            uint32_t right_sibling_page = *parent.internal_node_child(own_index + 1);
            if (can_borrow_from(right_sibling_page)) {
                return borrow_internal_from_right(page_num, right_sibling_page, own_index);
            }
        }

        if (own_index > 0) {
            merge_internal_nodes(parent_page_num, own_index - 1, own_index);
        } else {
            merge_internal_nodes(parent_page_num, own_index, own_index + 1);
        }
    }

    void borrow_internal_from_left(uint32_t receiver_page, uint32_t donor_page, uint32_t parent_key_index) {
        InternalNode receiver;
        receiver.node = (uint8_t*)this->table->pager->get_page(receiver_page);
        InternalNode donor;
        donor.node = (uint8_t*)this->table->pager->get_page(donor_page);
        InternalNode parent;
        parent.node = (uint8_t*)this->table->pager->get_page(*receiver.node_parent());

        uint32_t receiver_keys = *receiver.internal_node_num_keys();
        uint32_t donor_keys = *donor.internal_node_num_keys();

        // Shift receiver's keys/children to the right to make space at index 0
        for (uint32_t i = receiver_keys; i > 0; i--) {
            memcpy(receiver.internal_node_cell(i), receiver.internal_node_cell(i - 1), INTERNAL_NODE_CELL_SIZE);
        }

        *receiver.internal_node_child(0) = *receiver.internal_node_child(0); // This pointer is already there, but we need a key
        *receiver.internal_node_key(0) = *parent.internal_node_key(parent_key_index);

        uint32_t moving_child_page = *donor.internal_node_right_child();
        *receiver.internal_node_cell(0) = moving_child_page; // Set child pointer in index 0
        
        InternalNode movingChild;
        movingChild.node = (uint8_t*)this->table->pager->get_page(moving_child_page);
        *movingChild.node_parent() = receiver_page;

        *parent.internal_node_key(parent_key_index) = *donor.internal_node_key(donor_keys - 1);

        *donor.internal_node_right_child() = *donor.internal_node_child(donor_keys - 1);

        *receiver.internal_node_num_keys() = receiver_keys + 1;
        *donor.internal_node_num_keys() = donor_keys - 1;
    }

    void borrow_internal_from_right(uint32_t receiver_page, uint32_t donor_page, uint32_t parent_key_index) {
        InternalNode receiver;
        receiver.node = (uint8_t*)this->table->pager->get_page(receiver_page);
        InternalNode donor;
        donor.node = (uint8_t*)this->table->pager->get_page(donor_page);
        InternalNode parent;
        parent.node = (uint8_t*)this->table->pager->get_page(*receiver.node_parent());

        uint32_t receiver_keys = *receiver.internal_node_num_keys();
        uint32_t donor_keys = *donor.internal_node_num_keys();

        /* 
            The Parent's divider key moves DOWN to the receiver.
            It becomes a new key at the end of the receiver's current keys.
            The current right_child of the receiver becomes the child pointer for this new key.
        */
        *receiver.internal_node_child(receiver_keys) = *receiver.internal_node_right_child();
        *receiver.internal_node_key(receiver_keys) = *parent.internal_node_key(parent_key_index);

        uint32_t moving_child_page = *donor.internal_node_child(0);
        *receiver.internal_node_right_child() = moving_child_page;

        // Update the moving child's parent pointer to the receiver
        InternalNode movingChild;
        movingChild.node = (uint8_t*)this->table->pager->get_page(moving_child_page);
        *movingChild.node_parent() = receiver_page;

        *parent.internal_node_key(parent_key_index) = *donor.internal_node_key(0);
        for (uint32_t i = 0; i < donor_keys - 1; i++) {
            memcpy(donor.internal_node_cell(i), donor.internal_node_cell(i + 1), INTERNAL_NODE_CELL_SIZE);
        }
        *receiver.internal_node_num_keys() = receiver_keys + 1;
        *donor.internal_node_num_keys() = donor_keys - 1;
    }

    void merge_internal_nodes(uint32_t parent_page, uint32_t left_idx, uint32_t right_idx) {
        InternalNode parent;
        parent.node = (uint8_t*)this->table->pager->get_page(parent_page);
        
        uint32_t left_page_num = *parent.internal_node_child(left_idx);
        uint32_t right_page_num = *parent.internal_node_child(right_idx);
        
        InternalNode left;
        left.node = (uint8_t*)this->table->pager->get_page(left_page_num);
        InternalNode right;
        right.node = (uint8_t*)this->table->pager->get_page(right_page_num);

        uint32_t left_keys = *left.internal_node_num_keys();
        uint32_t right_keys = *right.internal_node_num_keys();

        *left.internal_node_key(left_keys) = *parent.internal_node_key(left_idx);
        *left.internal_node_child(left_keys + 1) = *right.internal_node_child(0); // Fixes the gap
        *left.internal_node_num_keys() += 1;
        left_keys++;

        // Append all keys and children from the right node
        for (uint32_t i = 0; i < right_keys; i++) {
            memcpy(left.internal_node_cell(left_keys + i), right.internal_node_cell(i), INTERNAL_NODE_CELL_SIZE);
            
            InternalNode movingChild;
            movingChild.node = (uint8_t*)this->table->pager->get_page(*right.internal_node_child(i));
            *movingChild.node_parent() = left_page_num;
        }
        
        *left.internal_node_right_child() = *right.internal_node_right_child();
        InternalNode lastChild;
        lastChild.node = (uint8_t*)this->table->pager->get_page(*left.internal_node_right_child());
        *lastChild.node_parent() = left_page_num;

        *left.internal_node_num_keys() += right_keys;
        remove_internal_node_key(parent, left_idx);
        internal_node_rebalance(parent_page);
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
        uint32_t old_max = oldInternalNode.get_node_max_key(this->table->pager);

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
            uint32_t new_max = oldInternalNode.get_node_max_key(this->table->pager);
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
