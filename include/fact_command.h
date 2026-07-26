#ifndef P101_MODULE_MAP_FACT_COMMAND_H
#define P101_MODULE_MAP_FACT_COMMAND_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stddef.h>

void p101_module_map_build_fact_command(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const struct arguments *args);

#endif    // P101_MODULE_MAP_FACT_COMMAND_H
