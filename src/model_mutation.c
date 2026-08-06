#include "model_mutation.h"
#include "errors.h"
#include "strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>

static struct module *p101_module_map_get_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *name);
static void           normalize_relative_path(const struct p101_env *env, char destination[MAX_NAME], const char *path);
static void           local_include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const struct source_file *file, const char *target);

static void normalize_relative_path(const struct p101_env *env, char destination[MAX_NAME], const char *path)
{
    const char *cursor;
    size_t      used;

    P101_TRACE_SCOPE(env);
    destination[0] = '\0';
    cursor         = path;
    used           = 0U;

    while(*cursor != '\0')
    {
        const char *component;
        size_t      component_length;

        while(*cursor == '/')
        {
            cursor++;
        }
        component = cursor;
        while(*cursor != '\0' && *cursor != '/')
        {
            cursor++;
        }
        component_length = (size_t)(cursor - component);

        if(component_length == 0U || (component_length == 1U && component[0] == '.'))
        {
            continue;
        }

        if(component_length == 2U && component[0] == '.' && component[1] == '.' && used > 0U)
        {
            size_t previous_start;

            previous_start = used;
            while(previous_start > 0U && destination[previous_start - 1U] != '/')
            {
                previous_start--;
            }
            if(used - previous_start != 2U || destination[previous_start] != '.' || destination[previous_start + 1U] != '.')
            {
                used              = previous_start == 0U ? 0U : previous_start - 1U;
                destination[used] = '\0';
                continue;
            }
        }

        /*
         * path is already bounded by MAX_NAME and lexical normalization can
         * only shorten it, so every copied component fits destination.
         */
        if(used > 0U)
        {
            destination[used++] = '/';
            destination[used]   = '\0';
        }
        for(size_t index = 0U; index < component_length; index++)
        {
            destination[used++] = component[index];
        }
        destination[used] = '\0';
    }
}

static void local_include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const struct source_file *file, const char *target)
{
    int p101_expression_result_2;
    int p101_call_result_3;
    int p101_call_result_4;
    p101_call_result_3 = p101_strncmp(env, target, "./", sizeof("./") - 1U);
    if(p101_call_result_3 == 0)
    {
        p101_expression_result_2 = 1;
    }
    else
    {
        p101_call_result_4 = p101_strncmp(env, target, "../", sizeof("../") - 1U);
        if(p101_call_result_4 == 0)
        {
            p101_expression_result_2 = 1;
        }
        else
        {
            p101_expression_result_2 = 0;
        }
    }
    if(p101_expression_result_2)
    {
        char        joined[MAX_NAME];
        char        normalized[MAX_NAME];
        const char *slash;
        size_t      directory_length;

        joined[0]        = '\0';
        slash            = p101_strrchr(env, file->module, '/');
        directory_length = slash == NULL ? 0U : (size_t)(slash - file->module);
        for(size_t index = 0U; index < directory_length && index + 1U < sizeof(joined); index++)
        {
            joined[index]     = file->module[index];
            joined[index + 1] = '\0';
        }
        if(directory_length > 0U)
        {
            p101_module_map_append_string(env, joined, sizeof(joined), "/");
        }
        p101_module_map_append_string(env, joined, sizeof(joined), target);
        normalize_relative_path(env, normalized, joined);
        p101_module_map_include_to_module(env, destination, destination_size, normalized);
    }
    else
    {
        p101_module_map_include_to_module(env, destination, destination_size, target);
    }
}

static struct module *p101_module_map_get_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *name)
{
    int            p101_call_result_1;
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = NULL;

    for(size_t i = 0; i < map->module_count; i++)
    {
        p101_call_result_1 = p101_strcmp(env, map->modules[i].name, name);
        if(p101_call_result_1 == 0)
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

    P101_TRACE_SCOPE(env);
    p101_module_map_basename_no_suffix(env, module_name, sizeof(module_name), path);
    p101_module_map_add_named_source_file(env, err, map, path, module_name, is_header);
}

void p101_module_map_add_named_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, const char *module_name, bool is_header)
{
    struct source_file *file;
    struct module      *module;

    P101_TRACE_SCOPE(env);
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

    P101_TRACE_SCOPE(env);
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
        local_include_to_module(env, include->target, sizeof(include->target), file, target);
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

void p101_module_map_add_function(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, const char *usr, size_t line, bool is_static, bool is_header_declaration)
{
    int                     p101_expression_result_5;
    size_t                  p101_call_result_6;
    struct function_record *function;
    struct module          *module;

    P101_TRACE_SCOPE(env);
    if(map->function_count >= MAX_FUNCTIONS)
    {
        P101_ERROR_RAISE_USER(err, "Too many functions for p101-module-map.", ERR_USAGE);
        goto done;
    }
    if(usr == NULL)
    {
        p101_expression_result_5 = 1;
    }
    else
    {
        p101_call_result_6 = p101_strlen(env, usr);
        if(p101_call_result_6 >= sizeof(map->functions[0].usr))
        {
            p101_expression_result_5 = 1;
        }
        else
        {
            p101_expression_result_5 = 0;
        }
    }
    if(p101_expression_result_5)
    {
        P101_ERROR_RAISE_USER(err, "A resolved function identity is absent or too long for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    function = &map->functions[map->function_count++];
    p101_module_map_copy_string(env, function->name, sizeof(function->name), name);
    p101_module_map_copy_string(env, function->usr, sizeof(function->usr), usr);
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

    P101_TRACE_SCOPE(env);
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

    P101_TRACE_SCOPE(env);
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

void p101_module_map_add_call(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, const char *usr, size_t line)
{
    struct call_record *call;
    size_t              usr_length;

    P101_TRACE_SCOPE(env);
    usr_length = 0U;
    if(map->call_count >= MAX_CALLS)
    {
        map->calls_dropped++;
        P101_ERROR_RAISE_USER(err, "Too many call facts for p101-module-map; refusing an incomplete analysis.", ERR_USAGE);
        goto done;
    }
    if(usr != NULL)
    {
        usr_length = p101_strlen(env, usr);
    }
    if(name == NULL || usr == NULL || usr_length >= sizeof(map->calls[0].usr))
    {
        P101_ERROR_RAISE_USER(err, "A resolved call identity is absent or too long for p101-module-map.", ERR_USAGE);
        goto done;
    }

    call = &map->calls[map->call_count++];
    p101_module_map_copy_string(env, call->name, sizeof(call->name), name);
    p101_module_map_copy_string(env, call->usr, sizeof(call->usr), usr);
    p101_module_map_copy_string(env, call->module, sizeof(call->module), file->module);
    p101_module_map_copy_string(env, call->path, sizeof(call->path), file->path);
    call->line      = line;
    call->is_header = file->is_header;

done:
    return;
}
