#include <iostream>
#include "pager.h"
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)
#ifndef TABLE_H
#define TABLE_H
using namespace std;

extern const uint32_t ID_SIZE;
extern const uint32_t USERNAME_SIZE;
extern const uint32_t EMAIL_SIZE;
extern const uint32_t ID_OFFSET;
extern const uint32_t USERNAME_OFFSET;
extern const uint32_t EMAIL_OFFSET;
extern const uint32_t ROW_SIZE;
extern const uint32_t PAGE_SIZE;
extern const uint32_t ROWS_PER_PAGE;
extern const uint32_t TABLE_MAX_ROWS;

class Table {
public: 
    uint32_t num_rows;              // Number of rows in the table
    // void* pages[TABLE_MAX_PAGES];   // Pointers to each page that contains rows
    Pager* pager;                   

    Table(const char* filename) {
        // for(uint32_t i=0; i<TABLE_MAX_PAGES; i++) this->pages[i] = nullptr;
        this->pager = new Pager(filename);      // This acts as open_db() since we only have 1 table in this proj
        this->num_rows = pager->file_length / ROW_SIZE;
    }

    void db_close() {
        Pager* pager = this->pager;
        uint32_t num_full_pages = this->num_rows / ROWS_PER_PAGE;

        for (uint32_t i = 0; i < num_full_pages; i++) {
            if (pager->pages[i] == NULL) {
                continue;
            }
            pager->pager_flush(i, PAGE_SIZE);
            free(pager->pages[i]);
            pager->pages[i] = NULL;
        }

        // There may be a partial page to write to the end of the file
        // This should not be needed after we switch to a B-tree
        uint32_t num_additional_rows = this->num_rows % ROWS_PER_PAGE;
        if (num_additional_rows > 0) {
            uint32_t page_num = num_full_pages;
            if (pager->pages[page_num] != NULL) {
                pager->pager_flush(page_num, num_additional_rows * ROW_SIZE);
                free(pager->pages[page_num]);
                pager->pages[page_num] = NULL;
            }
        }

        int result = close(pager->file_descriptor);
        if (result == -1) {
            printf("Error closing db file.\n");
            exit(EXIT_FAILURE);
        }
        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
            void* page = pager->pages[i];
            if (page) {
                free(page);
                pager->pages[i] = NULL;
            }
        }
    }

};

// Each row of the table
class Row {
public:
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];    // Allocating 1 additional byte for the null character
    char email[COLUMN_EMAIL_SIZE + 1];

    void serialize_row(void* destination) {
        // Just to give the void pointer 1 byte of space, Clang is strict in these sceneiors 
        char* dest = static_cast<char*>(destination);

        memcpy(dest + ID_OFFSET, &(this->id), ID_SIZE);
        memcpy(dest + USERNAME_OFFSET, &(this->username), USERNAME_SIZE);
        memcpy(dest + EMAIL_OFFSET, &(this->email), EMAIL_SIZE);
    }

    void deserialize_row(void* source) {
        char* src = static_cast<char*>(source);

        memcpy(&(this->id), src + ID_OFFSET, ID_SIZE);
        memcpy(&(this->username), src + USERNAME_OFFSET, USERNAME_SIZE);
        memcpy(&(this->email), src + EMAIL_OFFSET, EMAIL_SIZE);
    }

    void* row_slot(Table* table, uint32_t row_num) {
        uint32_t page_num = row_num / ROWS_PER_PAGE;
        // void* page = table->pages[page_num];
        // if (page == nullptr) {
        //     // Allocate memory only when we try to access page
        //     page = table->pages[page_num] = malloc(PAGE_SIZE);
        // }

        //Reading from pager now
        void* page = table->pager->get_page(page_num);
        uint32_t row_offset = row_num % ROWS_PER_PAGE;
        uint32_t byte_offset = row_offset * ROW_SIZE;

        // giving page, a void pointer, 1 byte space
        return static_cast<char*>(page) + byte_offset;
    }

    void print_row() {
        cout<<this->id<<" "<<this->username<<" "<<this->email<<"\n";
    }
};

// I have serialized the rows, so I need to have the offset for each column
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;
// Storing each row in a serialized blocks of memory , pages.
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;


#endif
