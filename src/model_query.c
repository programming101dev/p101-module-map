#include "model_query.h"
#include <p101_c/p101_string.h>

bool p101_module_map_module_has_direct_include(const struct p101_env *env, const struct project_map *map, const char *from, const char *target)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->include_count; i++)
    {
        if(map->includes[i].is_local && p101_strcmp(env, map->includes[i].from_module, from) == 0 && p101_strcmp(env, map->includes[i].target, target) == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_symbol_used_outside_module(const struct p101_env *env, const struct project_map *map, const char *module_name, const char *symbol_name)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->call_count; i++)
    {
        if(p101_strcmp(env, map->calls[i].module, module_name) != 0 && p101_strcmp(env, map->calls[i].name, symbol_name) == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_include_target_exists(const struct p101_env *env, const struct project_map *map, const char *target)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->module_count; i++)
    {
        if(p101_strcmp(env, map->modules[i].name, target) == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_has_call_named(const struct p101_env *env, const struct project_map *map, const char *name)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->call_count; i++)
    {
        if(p101_strcmp(env, map->calls[i].name, name) == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_module_used_outside_module(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->include_count; i++)
    {
        if(map->includes[i].is_local && p101_strcmp(env, map->includes[i].target, module_name) == 0 && p101_strcmp(env, map->includes[i].from_module, module_name) != 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_function_used_outside_module(const struct p101_env *env, const struct project_map *map, const struct function_record *function)
{
    bool ret_val;

    ret_val = false;
    if(!p101_module_map_module_used_outside_module(env, map, function->module))
    {
        goto done;
    }

    ret_val = p101_module_map_function_has_header_declaration(env, map, function);

done:
    return ret_val;
}

bool p101_module_map_function_has_header_declaration(const struct p101_env *env, const struct project_map *map, const struct function_record *function)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->function_count; i++)
    {
        if(map->functions[i].is_header_declaration && p101_strcmp(env, map->functions[i].name, function->name) == 0 && p101_strcmp(env, map->functions[i].module, function->module) == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_function_has_non_static_definition(const struct p101_env *env, const struct project_map *map, const struct function_record *function)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->function_count; i++)
    {
        if(!map->functions[i].is_header_declaration && !map->functions[i].is_static && p101_strcmp(env, map->functions[i].name, function->name) == 0 && p101_strcmp(env, map->functions[i].module, function->module) == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}
