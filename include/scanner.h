#ifndef P101_MODULE_MAP_SCANNER_H
#define P101_MODULE_MAP_SCANNER_H

#include "model.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_module_map_scan_path(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path);
void p101_module_map_parse_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file);

#endif    // P101_MODULE_MAP_SCANNER_H
