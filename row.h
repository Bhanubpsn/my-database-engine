#include <cstdint>
#include <cstring>
#include <iostream>
#include "types.h"
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)
#ifndef ROW_H
#define ROW_H
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
