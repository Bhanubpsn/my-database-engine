#ifndef TYPES_H
#define TYPES_H

// META commands are Non-SQL which starts with a '.' eg. ".exit"
typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

// For the actual SQL commands
typedef enum { 
    PREPARE_SUCCESS, 
    PREPARE_UNRECOGNIZED_STATEMENT 
} PrepareResult;

// Actual SQL commands, for now allowing only insert and select for the sake of checking wether it is working or not
typedef enum { 
    STATEMENT_INSERT, 
    STATEMENT_SELECT 
} StatementType;

#endif
