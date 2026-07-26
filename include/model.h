#ifndef P101_MODULE_MAP_MODEL_H
#define P101_MODULE_MAP_MODEL_H

#include "arguments.h"
#include "constants.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

struct source_file
{
    char path[PATH_LEN];
    char module[MAX_NAME];
    bool is_header;
};

struct module
{
    char   name[MAX_NAME];
    char   source_path[PATH_LEN];
    char   header_path[PATH_LEN];
    size_t source_count;
    size_t header_count;
    size_t function_count;
    size_t public_function_count;
    size_t static_function_count;
    size_t header_declaration_count;
    size_t macro_count;
    size_t type_count;
    size_t local_include_count;
    size_t external_include_count;
    bool   uses_error_object;
    bool   checks_error_object;
    bool   creates_error_object;
    bool   destroys_error_object;
    bool   creates_env_object;
    bool   destroys_env_object;
};

struct function_record
{
    char   name[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    bool   is_static;
    bool   is_header_declaration;
};

struct include_record
{
    char   from_module[MAX_NAME];
    char   target[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    bool   is_local;
};

struct macro_record
{
    char   name[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
};

struct type_record
{
    char   name[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
};

struct call_record
{
    char   name[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    bool   is_header;
};

struct project_map
{
    struct source_file     files[MAX_FILES];
    struct module          modules[MAX_MODULES];
    struct function_record functions[MAX_FUNCTIONS];
    struct include_record  includes[MAX_INCLUDES];
    struct macro_record    macros[MAX_MACROS];
    struct type_record     types[MAX_TYPES];
    struct call_record     calls[MAX_CALLS];
    size_t                 file_count;
    size_t                 module_count;
    size_t                 function_count;
    size_t                 include_count;
    size_t                 macro_count;
    size_t                 type_count;
    size_t                 call_count;
};

#endif    // P101_MODULE_MAP_MODEL_H
