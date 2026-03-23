#include <iostream>
#include "pager.h"
#include "row.h"
#ifndef TABLE_H
#define TABLE_H
using namespace std;

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

#endif
