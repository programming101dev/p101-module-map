#include "model_notes.h"
#include "errors.h"
#include "strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>

static struct module *p101_module_map_get_note_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file);

static struct module *p101_module_map_get_note_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = NULL;

    for(size_t i = 0; i < map->module_count; i++)
    {
        if(p101_strcmp(env, map->modules[i].name, file->module) == 0)
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
    p101_module_map_copy_string(env, module->name, sizeof(module->name), file->module);

done:
    return module;
}

void p101_module_map_note_error_use(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = p101_module_map_get_note_module(env, err, map, file);
    if(module != NULL)
    {
        module->uses_error_object = true;
    }
}

void p101_module_map_note_error_check(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = p101_module_map_get_note_module(env, err, map, file);
    if(module != NULL)
    {
        module->checks_error_object = true;
    }
}

void p101_module_map_note_error_create(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = p101_module_map_get_note_module(env, err, map, file);
    if(module != NULL)
    {
        module->creates_error_object = true;
    }
}

void p101_module_map_note_error_destroy(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = p101_module_map_get_note_module(env, err, map, file);
    if(module != NULL)
    {
        module->destroys_error_object = true;
    }
}

void p101_module_map_note_env_create(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = p101_module_map_get_note_module(env, err, map, file);
    if(module != NULL)
    {
        module->creates_env_object = true;
    }
}

void p101_module_map_note_env_destroy(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = p101_module_map_get_note_module(env, err, map, file);
    if(module != NULL)
    {
        module->destroys_env_object = true;
    }
}
