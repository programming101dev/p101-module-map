#include "model_mutation.h"
#include "errors.h"
#include "strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>

static struct module *p101_module_map_get_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *name);

static struct module *p101_module_map_get_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *name)
{
    struct module *module;

    P101_TRACE(env);
    module = NULL;

    for(size_t i = 0; i < map->module_count; i++)
    {
        if(p101_strcmp(env, map->modules[i].name, name) == 0)
        {
            module = &map->modules[i];
            goto done;
        }
    }

    if(map->module_count >= MAX_MODULES)
    {
        P101_ERROR_RAISE_USER(err, "Too many modules for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = &map->modules[map->module_count++];
    p101_memset(env, module, 0, sizeof(*module));
    p101_module_map_copy_string(env, module->name, sizeof(module->name), name);

done:
    return module;
}

void p101_module_map_add_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, bool is_header)
{
    char module_name[MAX_NAME];

    P101_TRACE(env);
    p101_module_map_basename_no_suffix(env, module_name, sizeof(module_name), path);
    p101_module_map_add_named_source_file(env, err, map, path, module_name, is_header);
}

void p101_module_map_add_named_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, const char *module_name, bool is_header)
{
    struct source_file *file;
    struct module      *module;

    P101_TRACE(env);
    if(map->file_count >= MAX_FILES)
    {
        P101_ERROR_RAISE_USER(err, "Too many files for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, module_name);
    if(module == NULL)
    {
        goto done;
    }

    file = &map->files[map->file_count++];
    p101_module_map_copy_string(env, file->path, sizeof(file->path), path);
    p101_module_map_copy_string(env, file->module, sizeof(file->module), module_name);
    file->is_header = is_header;

    if(is_header)
    {
        module->header_count++;
        if(module->header_path[0] == '\0')
        {
            p101_module_map_copy_string(env, module->header_path, sizeof(module->header_path), path);
        }
    }
    else
    {
        module->source_count++;
        if(module->source_path[0] == '\0')
        {
            p101_module_map_copy_string(env, module->source_path, sizeof(module->source_path), path);
        }
    }

done:
    return;
}

void p101_module_map_add_include(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *target, size_t line, bool is_local)
{
    struct include_record *include;
    struct module         *module;

    P101_TRACE(env);
    if(map->include_count >= MAX_INCLUDES)
    {
        P101_ERROR_RAISE_USER(err, "Too many includes for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    include = &map->includes[map->include_count++];
    p101_module_map_copy_string(env, include->from_module, sizeof(include->from_module), file->module);
    p101_module_map_copy_string(env, include->path, sizeof(include->path), file->path);
    include->line     = line;
    include->is_local = is_local;

    if(is_local)
    {
        p101_module_map_include_to_module(env, include->target, sizeof(include->target), target);
        module->local_include_count++;
    }
    else
    {
        p101_module_map_copy_string(env, include->target, sizeof(include->target), target);
        module->external_include_count++;
    }

done:
    return;
}

void p101_module_map_add_function(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line, bool is_static, bool is_header_declaration)
{
    struct function_record *function;
    struct module          *module;

    P101_TRACE(env);
    if(map->function_count >= MAX_FUNCTIONS)
    {
        P101_ERROR_RAISE_USER(err, "Too many functions for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    function = &map->functions[map->function_count++];
    p101_module_map_copy_string(env, function->name, sizeof(function->name), name);
    p101_module_map_copy_string(env, function->module, sizeof(function->module), file->module);
    p101_module_map_copy_string(env, function->path, sizeof(function->path), file->path);
    function->line                  = line;
    function->is_static             = is_static;
    function->is_header_declaration = is_header_declaration;

    if(is_header_declaration)
    {
        module->header_declaration_count++;
    }
    else
    {
        module->function_count++;
        if(is_static)
        {
            module->static_function_count++;
        }
        else
        {
            module->public_function_count++;
        }
    }

done:
    return;
}

void p101_module_map_add_macro(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line)
{
    struct macro_record *macro;
    struct module       *module;

    P101_TRACE(env);
    if(map->macro_count >= MAX_MACROS)
    {
        P101_ERROR_RAISE_USER(err, "Too many macros for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    macro = &map->macros[map->macro_count++];
    p101_module_map_copy_string(env, macro->name, sizeof(macro->name), name);
    p101_module_map_copy_string(env, macro->module, sizeof(macro->module), file->module);
    p101_module_map_copy_string(env, macro->path, sizeof(macro->path), file->path);
    macro->line = line;
    module->macro_count++;

done:
    return;
}

void p101_module_map_add_type(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line)
{
    struct type_record *type;
    struct module      *module;

    P101_TRACE(env);
    if(map->type_count >= MAX_TYPES)
    {
        P101_ERROR_RAISE_USER(err, "Too many types for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    type = &map->types[map->type_count++];
    p101_module_map_copy_string(env, type->name, sizeof(type->name), name);
    p101_module_map_copy_string(env, type->module, sizeof(type->module), file->module);
    p101_module_map_copy_string(env, type->path, sizeof(type->path), file->path);
    type->line = line;
    module->type_count++;

done:
    return;
}

void p101_module_map_add_call(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line)
{
    struct call_record *call;

    P101_TRACE(env);
    (void)err;
    if(map->call_count >= MAX_CALLS)
    {
        map->calls_dropped++;
        goto done;
    }

    call = &map->calls[map->call_count++];
    p101_module_map_copy_string(env, call->name, sizeof(call->name), name);
    p101_module_map_copy_string(env, call->module, sizeof(call->module), file->module);
    p101_module_map_copy_string(env, call->path, sizeof(call->path), file->path);
    call->line      = line;
    call->is_header = file->is_header;

done:
    return;
}
