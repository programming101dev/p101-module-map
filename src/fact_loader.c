#include "fact_loader.h"
#include "constants.h"
#include "errors.h"
#include "fact_command.h"
#include "model_mutation.h"
#include "model_notes.h"
#include "strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <p101_util/tool_run.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/wait.h>

static struct source_file *p101_module_map_find_fact_file(const struct p101_env *env, struct project_map *map, const char *path);
static struct source_file *p101_module_map_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact);
static void                p101_module_map_note_call_semantics(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name);
static void                p101_module_map_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact);
static bool                p101_module_map_fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line);

static struct source_file *p101_module_map_find_fact_file(const struct p101_env *env, struct project_map *map, const char *path)
{
    struct source_file *file;

    P101_TRACE_SCOPE(env);
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

    P101_TRACE_SCOPE(env);
    file = NULL;
    if(fact == NULL || fact->path == NULL)
    {
        goto done;
    }

    file = p101_module_map_find_fact_file(env, map, fact->path);
    if(file == NULL)
    {
        char module_name[MAX_NAME];

        p101_module_map_normalize_module_name(env, module_name, sizeof(module_name), fact->module);
        p101_module_map_add_named_source_file(env, err, map, fact->path, module_name, fact->is_header);
        file = p101_module_map_find_fact_file(env, map, fact->path);
    }

done:
    return file;
}

static void p101_module_map_note_call_semantics(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name)
{
    P101_TRACE_SCOPE(env);
    if(p101_strcmp(env, name, "p101_error_create") == 0 || p101_strcmp(env, name, "p101_error_destroy") == 0)
    {
        p101_module_map_note_error_use(env, err, map, file);
    }
    else if(p101_strncmp(env, name, "p101_error_has_", P101_ERROR_HAS_LEN) == 0 || p101_strncmp(env, name, "p101_error_is_", P101_ERROR_IS_LEN) == 0 || p101_strcmp(env, name, "p101_error_reset") == 0)
    {
        p101_module_map_note_error_use(env, err, map, file);
        p101_module_map_note_error_check(env, err, map, file);
    }
}

static void p101_module_map_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    const struct source_file *file;

    P101_TRACE_SCOPE(env);
    if(fact == NULL)
    {
        goto done;
    }

    file = p101_module_map_file_for_fact(env, err, map, fact);
    if(file == NULL)
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

static bool p101_module_map_fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line)
{
    bool   complete;
    size_t length;

    P101_TRACE_SCOPE(env);
    complete = true;
    length   = p101_strlen(env, line);

    if(length == MAX_LINE - 1U && p101_strchr(env, line, '\n') == NULL)
    {
        char discard[MAX_LINE];

        complete = false;
        while(p101_error_has_no_error(err) && p101_fgets(env, err, discard, sizeof(discard), stream) != NULL)
        {
            if(p101_strchr(env, discard, '\n') != NULL)
            {
                break;
            }
        }
    }

    return complete;
}

#ifdef P101_MODULE_MAP_TESTING
struct source_file *p101_module_map_test_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    return p101_module_map_file_for_fact(env, err, map, fact);
}

void p101_module_map_test_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    p101_module_map_apply_fact(env, err, map, fact);
}

bool p101_module_map_test_fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line)
{
    return p101_module_map_fact_line_is_complete(env, err, stream, line);
}
#endif

void p101_module_map_load_clang_facts(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct arguments *args)
{
    FILE                      *stream;
    char                       line[MAX_LINE];
    bool                       is_pipe;
    bool                       pipe_open;
    size_t                     fact_count;
    struct p101_tool_argv      command;
    struct p101_tool_read_pipe pipe_state = {NULL, -1};

    P101_TRACE_SCOPE(env);
    stream     = NULL;
    is_pipe    = args->facts_path == NULL;
    pipe_open  = false;
    fact_count = 0U;
    if(is_pipe)
    {
        p101_module_map_build_fact_argv(env, err, &command, args);
        if(p101_error_has_error(err))
        {
            goto done;
        }
        if(args->verbose)
        {
            p101_fputs(env, err, "p101-module-map: fact argv:", stderr);
            for(size_t index = 0U; index < command.count && p101_error_has_no_error(err); index++)
            {
                p101_fprintf(env, err, stderr, " [%s]", command.values[index]);
            }
            p101_fputc(env, err, '\n', stderr);
            if(p101_error_has_error(err))
            {
                goto done;
            }
        }
        pipe_open = p101_tool_read_pipe_open(env, err, command.values, "p101-module-map fact producer", true, &pipe_state);
        stream    = pipe_state.stream;
    }
    else
    {
        stream = p101_fopen(env, err, args->facts_path, "r");
    }
    if(stream == NULL)
    {
        goto done;
    }

    while(p101_fgets(env, err, line, sizeof(line), stream) != NULL && p101_error_has_no_error(err))
    {
        struct p101_c_fact      fact;
        enum p101_c_fact_status status;

        if(!p101_module_map_fact_line_is_complete(env, err, stream, line))
        {
            continue;
        }

        status = p101_c_fact_parse_line(env, err, line, &fact);
        if(status == P101_C_FACT_OTHER)
        {
            continue;
        }
        if(status != P101_C_FACT_OK)
        {
            if(p101_error_has_no_error(err))    // GCOVR_EXCL_BR_LINE -- parser errors already populate err
            {
                P101_ERROR_RAISE_USER(err, "p101-wrapper-audit emitted an invalid module fact record.", ERR_USAGE);
            }
            break;
        }

        p101_module_map_apply_fact(env, err, map, &fact);
        fact_count++;
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(is_pipe)
    {
        int status;

        status    = p101_tool_read_pipe_close(env, err, &pipe_state);
        stream    = NULL;
        pipe_open = false;
        if(p101_error_has_no_error(err) && (!WIFEXITED(status) || WEXITSTATUS(status) != 0))
        {
            P101_ERROR_RAISE_USER(err, "p101-wrapper-audit failed while emitting module facts.", ERR_USAGE);
        }
        if(p101_error_has_error(err))
        {
            goto done;
        }
    }
    else
    {
        p101_fclose(env, err, stream);
        if(p101_error_has_error(err))
        {
            stream = NULL;
            goto done;
        }
    }
    stream = NULL;
    if(fact_count == 0U)
    {
        P101_ERROR_RAISE_USER(err, "The fact stream did not contain any p101 C facts.", ERR_USAGE);
    }

done:
    if(stream != NULL)
    {
        if(pipe_open)
        {
            (void)p101_tool_read_pipe_close(env, NULL, &pipe_state);
        }
        else
        {
            p101_fclose(env, err, stream);
        }
    }
    if(is_pipe)
    {
        p101_tool_argv_destroy(env, &command);
    }
}
