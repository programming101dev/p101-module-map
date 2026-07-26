#ifndef P101_MODULE_MAP_ARGUMENTS_H
#define P101_MODULE_MAP_ARGUMENTS_H

#include <stdbool.h>
#include <stddef.h>

enum
{
    PATH_LEN                  = 1024,
    P101_MODULE_MAP_MAX_PATHS = 64
};

struct arguments
{
    const char *output_path;
    const char *paths[P101_MODULE_MAP_MAX_PATHS];
    size_t      path_count;
    size_t      max_functions;
    size_t      max_public;
    bool        verbose;
};

#endif    // P101_MODULE_MAP_ARGUMENTS_H
