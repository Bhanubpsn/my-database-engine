#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include "types.h"
#ifndef PAGER_H
#define PAGER_H

// To persist data on disk and read back from the disk in case of cache miss 

class Pager {
public:
    int file_descriptor;
    uint32_t file_length;
    void* pages[TABLE_MAX_PAGES];

    Pager (const char* filename) {
        int fd = open(filename,
                        O_RDWR |      // Read/Write mode
                        O_CREAT,      // Create file if it does not exist
                        S_IWUSR |     // User write permission
                        S_IRUSR       // User read permission
        );

        if (fd == -1) {
            printf("Unable to open file\n");
            exit(EXIT_FAILURE);
        }

        // long long 
        off_t file_length = lseek(fd, 0, SEEK_END);
        this->file_descriptor = fd;
        this->file_length = file_length;

        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
            this->pages[i] = NULL;
        }
    }

    // Reading from the file
    void* get_page(uint32_t page_num) {
        if (page_num > TABLE_MAX_PAGES) {
            printf("Tried to fetch page number out of bounds. %d > %d\n", page_num, TABLE_MAX_PAGES);
            exit(EXIT_FAILURE);
        }

        if (this->pages[page_num] == NULL) {
            // Cache miss. Allocate memory and load from file.
            void* page = malloc(PAGE_SIZE);
            uint32_t num_pages = this->file_length / PAGE_SIZE;

            // We might save a partial page at the end of the file
            if (this->file_length % PAGE_SIZE) {
                num_pages += 1;
            }

            if (page_num <= num_pages) {
                lseek(this->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
                ssize_t bytes_read = read(this->file_descriptor, page, PAGE_SIZE);
                if (bytes_read == -1) {
                    printf("Error reading file: %d\n", errno);
                    exit(EXIT_FAILURE);
                }
            }

            this->pages[page_num] = page;
        }

        return this->pages[page_num];
    }

    // Writing in the file
    void pager_flush(uint32_t page_num, uint32_t size) {
        if (this->pages[page_num] == NULL) {
            printf("Tried to flush null page\n");
            exit(EXIT_FAILURE);
        }

        off_t offset = lseek(this->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

        if (offset == -1) {
            printf("Error seeking: %d\n", errno);
            exit(EXIT_FAILURE);
        }

        ssize_t bytes_written =
            write(this->file_descriptor, this->pages[page_num], size);

        if (bytes_written == -1) {
            printf("Error writing: %d\n", errno);
            exit(EXIT_FAILURE);
        }
    }
};

#endif
