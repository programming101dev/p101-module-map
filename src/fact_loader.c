#include "fact_loader.h"
#include "constants.h"
#include "errors.h"
#include "fact_command.h"
#include "model_mutation.h"
#include "model_notes.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <p101_posix/p101_stdio.h>
#include <stdio.h>

static struct source_file *p101_module_map_find_fact_file(const struct p101_env *env, struct project_map *map, const char *path);
static struct source_file *p101_module_map_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact);
static void                p101_module_map_note_call_semantics(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name);
static void                p101_module_map_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact);

static struct source_file *p101_module_map_find_fact_file(const struct p101_env *env, struct project_map *map, const char *path)
{
    struct source_file *file;

    P101_TRACE(env);
    file = NULL;
    for(size_t i = 0; i < map->file_count; i++)
    {
        if(p101_strcmp(env, map->files[i].path, path) == 0)
        {
            file = &map->files[i];
            goto done;
        }
    }

done:
    return file;
}

static struct source_file *p101_module_map_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    struct source_file *file;

    P101_TRACE(env);
    file = NULL;
    if(fact == NULL || fact->path == NULL)
    {
        goto done;
    }

    file = p101_module_map_find_fact_file(env, map, fact->path);
    if(file == NULL)
    {
        p101_module_map_add_source_file(env, err, map, fact->path, fact->is_header);
        file = p101_module_map_find_fact_file(env, map, fact->path);
    }

done:
    return file;
}

static void p101_module_map_note_call_semantics(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name)
{
    P101_TRACE(env);
    if(p101_strcmp(env, name, "p101_error_create") == 0)
    {
        p101_module_map_note_error_use(env, err, map, file);
        p101_module_map_note_error_create(env, err, map, file);
    }
    else if(p101_strcmp(env, name, "p101_error_destroy") == 0)
    {
        p101_module_map_note_error_use(env, err, map, file);
        p101_module_map_note_error_destroy(env, err, map, file);
    }
    else if(p101_strncmp(env, name, "p101_error_has_", P101_ERROR_HAS_LEN) == 0 || p101_strncmp(env, name, "p101_error_is_", P101_ERROR_IS_LEN) == 0 || p101_strcmp(env, name, "p101_error_reset") == 0)
    {
        p101_module_map_note_error_use(env, err, map, file);
        p101_module_map_note_error_check(env, err, map, file);
    }
    else if(p101_strcmp(env, name, "p101_env_create") == 0)
    {
        p101_module_map_note_env_create(env, err, map, file);
    }
    else if(p101_strcmp(env, name, "p101_env_destroy") == 0)
    {
        p101_module_map_note_env_destroy(env, err, map, file);
    }
}

static void p101_module_map_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    const struct source_file *file;

    P101_TRACE(env);
    if(fact == NULL)
    {
        goto done;
    }

    file = p101_module_map_file_for_fact(env, err, map, fact);
    if(file == NULL || p101_error_has_error(err))
    {
        goto done;
    }

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(fact->kind)
    {
        case P101_C_FACT_KIND_UNKNOWN:
        case P101_C_FACT_KIND_FILE:
            break;
        case P101_C_FACT_KIND_INCLUDE:
            p101_module_map_add_include(env, err, map, file, fact->value, fact->line, fact->flag1);
            break;
        case P101_C_FACT_KIND_FUNCTION:
            p101_module_map_add_function(env, err, map, file, fact->value, fact->line, fact->flag1, fact->flag2);
            break;
        case P101_C_FACT_KIND_CALL:
            p101_module_map_add_call(env, err, map, file, fact->value, fact->line);
            p101_module_map_note_call_semantics(env, err, map, file, fact->value);
            break;
        case P101_C_FACT_KIND_TYPE:
            p101_module_map_add_type(env, err, map, file, fact->value, fact->line);
            break;
        case P101_C_FACT_KIND_MACRO:
            p101_module_map_add_macro(env, err, map, file, fact->value, fact->line);
            break;
        case P101_C_FACT_KIND_NOTE:
            if(p101_strcmp(env, fact->value, "ERROR_USE") == 0)
            {
                p101_module_map_note_error_use(env, err, map, file);
            }
            else if(p101_strcmp(env, fact->value, "ERROR_CHECK") == 0)
            {
                p101_module_map_note_error_use(env, err, map, file);
                p101_module_map_note_error_check(env, err, map, file);
            }
            break;
        default:
            break;
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

done:
    return;
}

void p101_module_map_load_clang_facts(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct arguments *args)
{
    FILE *stream;
    char  command[MAX_COMMAND];
    char  line[MAX_LINE];

    P101_TRACE(env);
    stream = NULL;
    p101_module_map_build_fact_command(env, err, command, sizeof(command), args);
    if(p101_error_has_error(err))
    {
        goto done;
    }
    if(args->verbose)
    {
        p101_fprintf(env, err, stderr, "p101-module-map: fact command: %s\n", command);
        if(p101_error_has_error(err))
        {
            goto done;
        }
    }

    stream = p101_popen(env, err, command, "r");
    if(stream == NULL)
    {
        goto done;
    }

    while(p101_fgets(env, err, line, sizeof(line), stream) != NULL && p101_error_has_no_error(err))
    {
        struct p101_c_fact      fact;
        enum p101_c_fact_status status;

        status = p101_c_fact_parse_line(env, err, line, &fact);
        if(status == P101_C_FACT_OTHER)
        {
            continue;
        }
        if(status != P101_C_FACT_OK)
        {
            if(p101_error_has_no_error(err))
            {
                P101_ERROR_RAISE_USER(err, "p101-wrapper-audit emitted an invalid module fact record.", ERR_USAGE);
            }
            break;
        }

        p101_module_map_apply_fact(env, err, map, &fact);
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(p101_pclose(env, err, stream) != 0)
    {
        stream = NULL;
        if(p101_error_has_no_error(err))
        {
            P101_ERROR_RAISE_USER(err, "p101-wrapper-audit failed while emitting module facts.", ERR_USAGE);
        }
        goto done;
    }
    stream = NULL;

done:
    if(stream != NULL)
    {
        (void)p101_pclose(env, NULL, stream);
    }
}
