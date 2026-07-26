#ifndef P101_MODULE_MAP_PARSE_H
#define P101_MODULE_MAP_PARSE_H

#include "constants.h"
#include <p101_env/env.h>
#include <stdbool.h>
#include <stddef.h>

bool p101_module_map_is_source_suffix(const struct p101_env *env, const char *path);
bool p101_module_map_is_header_suffix(const struct p101_env *env, const char *path);
bool p101_module_map_is_skipped_dir(const struct p101_env *env, const char *name);
bool p101_module_map_parse_include_line(const struct p101_env *env, char *target, size_t target_size, bool *is_local, const char *line);
bool p101_module_map_parse_macro_line(const struct p101_env *env, char *name, size_t name_size, const char *line);
bool p101_module_map_parse_type_line(const struct p101_env *env, char *name, size_t name_size, const char *line);
bool p101_module_map_parse_function_signature(const struct p101_env *env, char *name, size_t name_size, bool *is_static, const char *signature);
bool p101_module_map_looks_like_control_keyword(const struct p101_env *env, const char *name);

#endif    // P101_MODULE_MAP_PARSE_H
