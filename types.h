#ifndef TYPES_H
#define TYPES_H
#define TABLE_MAX_PAGES 100
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define INVALID_PAGE_NUM UINT32_MAX

const uint32_t PAGE_SIZE = 4096;

// META commands are Non-SQL which starts with a '.' eg. ".exit"
typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

// For the actual SQL commands
typedef enum { 
    PREPARE_SUCCESS, 
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_SYNTAX_ERROR,
    PREPARE_STRING_TOO_LONG,
    PREPARE_NEGATIVE_ID,
} PrepareResult;

// Actual SQL commands, for now allowing only insert, select, update for the sake of checking wether it is working or not
typedef enum { 
    STATEMENT_INSERT, 
    STATEMENT_SELECT,
    STATEMENT_UPDATE,
} StatementType;

typedef enum { 
    EXECUTE_SUCCESS, 
    EXECUTE_TABLE_FULL,
    EXECUTE_DUPLICATE_KEY,
    EXECUTE_KEY_NOT_FOUND,
} ExecuteResult;

// B-Tree Nodes 
typedef enum { 
    NODE_INTERNAL, 
    NODE_LEAF 
} NodeType;

#endif
