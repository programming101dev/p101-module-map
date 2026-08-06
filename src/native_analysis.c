#include "native_analysis.h"
#include "constants.h"
#include "errors.h"
#include "model_mutation.h"
#include "model_notes.h"
#include "strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/analysis.h>
#include <p101_c_facts/project.h>

static bool                apply_record(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context);
static struct source_file *file_for_record(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_analysis_record *record);

void p101_module_map_load_native_analysis(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct arguments *args)
{
    int                            p101_expression_result_8;
    bool                           p101_call_result_9;
    bool                           p101_call_result_1;
    bool                           p101_call_result_2;
    bool                           p101_call_result_3;
    static const char              default_path[] = ".";
    const char                    *paths[P101_MODULE_MAP_MAX_PATHS];
    size_t                         path_count;
    char                           discovered_compile_db[PATH_LEN];
    const char                    *compile_db;
    struct p101_c_analysis_options options;

    P101_TRACE_SCOPE(env);
    path_count = args->path_count;
    if(path_count == 0U)
    {
        paths[0]   = default_path;
        path_count = 1U;
    }
    else
    {
        for(size_t index = 0U; index < path_count; index++)
        {
            paths[index] = args->paths[index];
        }
    }

    compile_db               = args->compile_db_path;
    p101_expression_result_8 = 0;
    if(compile_db == NULL)
    {
        p101_call_result_9 = p101_c_facts_find_clang_compile_database(env, err, ".", discovered_compile_db, sizeof(discovered_compile_db));
        if(p101_call_result_9)
        {
            p101_expression_result_8 = 1;
        }
    }
    if(p101_expression_result_8)
    {
        compile_db = discovered_compile_db;
    }
    p101_call_result_1 = p101_error_has_error(err);
    if(p101_call_result_1)
    {
        goto done;
    }

    p101_memset(env, &options, 0, sizeof(options));
    options.compile_database                     = compile_db;
    options.paths                                = paths;
    options.path_count                           = path_count;
    options.compile_database_only                = compile_db != NULL;
    options.detailed_preprocessing               = true;
    options.include_headers_as_translation_units = false;
    options.keep_going                           = false;
    if(args->verbose)
    {
        p101_fprintf(env, err, stderr, "p101-module-map: native lib_c_facts scan (%zu path%s%s)\n", path_count, path_count == 1U ? "" : "s", compile_db == NULL ? "" : ", compile database active");
    }
    p101_call_result_2 = p101_error_has_no_error(err);
    if(p101_call_result_2)
    {
        p101_call_result_3 = p101_c_analysis_scan(env, err, &options, apply_record, map);
        (void)p101_call_result_3;
    }

done:
    return;
}

static bool apply_record(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context)
{
    int                       p101_call_result_7;
    int                       p101_call_result_6;
    struct project_map       *map;
    const struct source_file *file;
    bool                      keep_going;

    P101_TRACE_SCOPE(env);
    map        = (struct project_map *)context;
    keep_going = false;
    if(record->kind == P101_C_ANALYSIS_DIAGNOSTIC)
    {
        P101_ERROR_RAISE_USER(err, record->name == NULL ? "lib_c_facts could not parse an admitted source file." : record->name, ERR_USAGE);
        goto done;
    }

    file = file_for_record(env, err, map, record);
    if(file == NULL)
    {
        keep_going = p101_error_has_no_error(err);
        goto done;
    }

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(record->kind)
    {
        case P101_C_ANALYSIS_FILE:
        case P101_C_ANALYSIS_MUTATION:
        case P101_C_ANALYSIS_DIAGNOSTIC:
            break;
        case P101_C_ANALYSIS_INCLUDE:
            p101_module_map_add_include(env, err, map, file, record->name, record->line, record->is_local_include);
            break;
        case P101_C_ANALYSIS_FUNCTION:
        {
            bool declaration;

            declaration = true;
            if(record->is_definition)
            {
                declaration = false;
            }
            p101_module_map_add_function(env, err, map, file, record->name, record->usr, record->line, record->is_static, declaration);
            break;
        }
        case P101_C_ANALYSIS_CALL:
            p101_module_map_add_call(env, err, map, file, record->name, record->usr, record->line);
            break;
        case P101_C_ANALYSIS_TYPE:
        case P101_C_ANALYSIS_ENUM:
            p101_module_map_add_type(env, err, map, file, record->name, record->line);
            break;
        case P101_C_ANALYSIS_ENUMERATOR:
            break;
        case P101_C_ANALYSIS_MACRO:
            p101_module_map_add_macro(env, err, map, file, record->name, record->line);
            break;
        case P101_C_ANALYSIS_NOTE:
        {
            p101_call_result_6 = p101_strcmp(env, record->name, "ERROR_USE");
            if(p101_call_result_6 == 0)
            {
                p101_module_map_note_error_use(env, err, map, file);
            }
            else
            {
                p101_call_result_7 = p101_strcmp(env, record->name, "ERROR_CHECK");
                if(p101_call_result_7 == 0)
                {
                    p101_module_map_note_error_use(env, err, map, file);
                    p101_module_map_note_error_check(env, err, map, file);
                }
            }
        }
        break;
        default:
            break;
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
    keep_going = p101_error_has_no_error(err);

done:
    return keep_going;
}

static struct source_file *file_for_record(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_analysis_record *record)
{
    int                 p101_call_result_4;
    int                 p101_call_result_5;
    struct source_file *file;
    char                module_name[MAX_NAME];

    file = NULL;
    if(record->path == NULL)
    {
        goto done;
    }
    for(size_t index = 0U; index < map->file_count; index++)
    {
        p101_call_result_4 = p101_strcmp(env, map->files[index].path, record->path);
        if(p101_call_result_4 == 0)
        {
            file = &map->files[index];
            goto done;
        }
    }

    p101_module_map_normalize_module_name(env, module_name, sizeof(module_name), record->path);
    p101_module_map_add_named_source_file(env, err, map, record->path, module_name, record->is_header);
    for(size_t index = 0U; index < map->file_count; index++)
    {
        p101_call_result_5 = p101_strcmp(env, map->files[index].path, record->path);
        if(p101_call_result_5 == 0)
        {
            file = &map->files[index];
            break;
        }
    }

done:
    return file;
}
