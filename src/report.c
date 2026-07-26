#include "report.h"
#include "model.h"
#include "model_query.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdio.h>

struct raw_call_rule
{
    const char *name;
    const char *library;
};

static const struct raw_call_rule RAW_CALL_RULES[] = {
    {"calloc",   "lib_c"    },
    {"closedir", "lib_posix"},
    {"close",    "lib_posix"},
    {"fclose",   "lib_c"    },
    {"fdopen",   "lib_posix"},
    {"fflush",   "lib_c"    },
    {"fgetc",    "lib_c"    },
    {"fgets",    "lib_c"    },
    {"fopen",    "lib_c"    },
    {"fprintf",  "lib_c"    },
    {"fputc",    "lib_c"    },
    {"fputs",    "lib_c"    },
    {"free",     "lib_c"    },
    {"malloc",   "lib_c"    },
    {"memcpy",   "lib_c"    },
    {"memset",   "lib_c"    },
    {"open",     "lib_posix"},
    {"opendir",  "lib_posix"},
    {"printf",   "lib_c"    },
    {"read",     "lib_posix"},
    {"realloc",  "lib_c"    },
    {"snprintf", "lib_c"    },
    {"strcmp",   "lib_c"    },
    {"strcpy",   "lib_c"    },
    {"strlen",   "lib_c"    },
    {"strncmp",  "lib_c"    },
    {"strncpy",  "lib_c"    },
    {"write",    "lib_posix"},
};

static bool        p101_module_map_is_platform_specific_include(const struct p101_env *env, const char *target);
static const char *p101_module_map_raw_call_library(const struct p101_env *env, const char *name);

static bool p101_module_map_is_platform_specific_include(const struct p101_env *env, const char *target)
{
    bool ret_val;

    ret_val = false;
    if(p101_strncmp(env, target, "linux/", sizeof("linux/") - 1U) == 0 || p101_strncmp(env, target, "mach/", sizeof("mach/") - 1U) == 0 || p101_strncmp(env, target, "windows", sizeof("windows") - 1U) == 0 || p101_strcmp(env, target, "sys/event.h") == 0 ||
       p101_strcmp(env, target, "sys/kqueue.h") == 0 || p101_strcmp(env, target, "sys/sysctl.h") == 0)
    {
        ret_val = true;
    }

    return ret_val;
}

static const char *p101_module_map_raw_call_library(const struct p101_env *env, const char *name)
{
    const char *library;

    library = NULL;
    for(size_t i = 0; i < sizeof(RAW_CALL_RULES) / sizeof(RAW_CALL_RULES[0]); i++)
    {
        if(p101_strcmp(env, RAW_CALL_RULES[i].name, name) == 0)
        {
            library = RAW_CALL_RULES[i].library;
            break;
        }
    }

    return library;
}

void p101_module_map_write_report(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map)
{
    P101_TRACE(env);
    p101_fputs(env, err, "# p101 module map\n\n", stream);
    p101_fprintf(env, err, stream, "Files scanned: `%zu`\n\n", map->file_count);
    p101_fprintf(env, err, stream, "Modules found: `%zu`\n\n", map->module_count);
    p101_fprintf(env, err, stream, "Functions found: `%zu`\n\n", map->function_count);
    p101_module_map_write_modules(env, err, stream, map);
    p101_module_map_write_include_graph(env, err, stream, map);
    p101_module_map_write_findings(env, err, stream, args, map);
}

void p101_module_map_write_modules(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map)
{
    P101_TRACE(env);
    p101_fputs(env, err, "## Modules\n\n", stream);
    p101_fputs(env, err, "| Module | Source | Header | Functions | Public | Static | Header declarations | Macros | Types | Includes |\n", stream);
    p101_fputs(env, err, "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n", stream);

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;

        module = &map->modules[i];
        p101_fprintf(env,
                     err,
                     stream,
                     "| `%s` | %zu | %zu | %zu | %zu | %zu | %zu | %zu | %zu | %zu local / %zu external |\n",
                     module->name,
                     module->source_count,
                     module->header_count,
                     module->function_count,
                     module->public_function_count,
                     module->static_function_count,
                     module->header_declaration_count,
                     module->macro_count,
                     module->type_count,
                     module->local_include_count,
                     module->external_include_count);
    }

    p101_fputs(env, err, "\n## Functions by module\n\n", stream);
    for(size_t i = 0; i < map->module_count; i++)
    {
        p101_fprintf(env, err, stream, "### `%s`\n\n", map->modules[i].name);
        p101_module_map_write_functions_for_module(env, err, stream, map, map->modules[i].name);
    }
}

void p101_module_map_write_include_graph(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map)
{
    P101_TRACE(env);
    p101_fputs(env, err, "## Local include graph\n\n", stream);

    for(size_t i = 0; i < map->include_count; i++)
    {
        const struct include_record *include;

        include = &map->includes[i];
        if(include->is_local)
        {
            p101_fprintf(env, err, stream, "- `%s` -> `%s` (%s:%zu)\n", include->from_module, include->target, include->path, include->line);
        }
    }

    p101_fputs(env, err, "\n", stream);
}

void p101_module_map_write_findings(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map)
{
    bool wrote;

    P101_TRACE(env);
    wrote = false;
    p101_fputs(env, err, "## Teaching notes\n\n", stream);

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;

        module = &map->modules[i];
        if(module->source_count > 0U && module->header_count == 0U && p101_strcmp(env, module->name, "main") != 0)
        {
            p101_fprintf(env, err, stream, "- `%s` has source code but no matching header. If other modules should use it, create a small public interface.\n", module->name);
            wrote = true;
        }

        if(module->function_count > args->max_functions)
        {
            p101_fprintf(env, err, stream, "- `%s` has %zu functions. Consider splitting one responsibility into another module.\n", module->name, module->function_count);
            wrote = true;
        }

        if(module->public_function_count > args->max_public)
        {
            p101_fprintf(env, err, stream, "- `%s` exposes %zu non-static functions. Consider making helpers `static` or narrowing the public API.\n", module->name, module->public_function_count);
            wrote = true;
        }

        if(p101_strcmp(env, module->name, "main") == 0 && module->function_count > 3U)
        {
            p101_fprintf(env, err, stream, "- `main` has %zu functions. Students usually do better when `main.c` only wires argument parsing, setup, and top-level control flow.\n", module->function_count);
            wrote = true;
        }

        if((p101_strcmp(env, module->name, "util") == 0 || p101_strcmp(env, module->name, "utils") == 0) && module->function_count > 4U)
        {
            p101_fprintf(env, err, stream, "- `%s` looks like a utility dumping ground. Try naming modules after responsibilities instead.\n", module->name);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;

        function = &map->functions[i];
        if(!function->is_header_declaration && !function->is_static && !p101_module_map_function_used_outside_module(env, map, function) && p101_strcmp(env, function->name, "main") != 0)
        {
            p101_fprintf(env, err, stream, "- `%s` in `%s` is non-static but does not appear to be part of a used module interface. Prefer `static`; only leave it public when another module must call it through the header.\n", function->name, function->path);
            wrote = true;
        }

        if(function->is_header_declaration && !p101_module_map_function_has_non_static_definition(env, map, function))
        {
            p101_fprintf(env, err, stream, "- `%s` is declared in `%s`, but no matching non-static definition was found. Remove the declaration or make the implementation match the public API.\n", function->name, function->path);
            wrote = true;
        }

        if(function->is_header_declaration && !p101_module_map_module_used_outside_module(env, map, function->module) && p101_strcmp(env, function->module, "main") != 0)
        {
            p101_fprintf(env, err, stream, "- `%s` is declared in `%s`, but no other module includes `%s`'s interface. Keep helpers `static` and remove header declarations until another file needs them.\n", function->name, function->path, function->module);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->macro_count; i++)
    {
        const struct macro_record *macro;

        macro = &map->macros[i];
        if(!p101_module_map_module_used_outside_module(env, map, macro->module) && !p101_module_map_symbol_used_outside_module(env, map, macro->module, macro->name))
        {
            p101_fprintf(env,
                         err,
                         stream,
                         "- Macro `%s` is exposed in `%s`, but the module interface does not appear to be used outside `%s`. Prefer a private enum/constant in the `.c` file until another module needs it.\n",
                         macro->name,
                         macro->path,
                         macro->module);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->type_count; i++)
    {
        const struct type_record *type;

        type = &map->types[i];
        if(!p101_module_map_module_used_outside_module(env, map, type->module))
        {
            p101_fprintf(env, err, stream, "- Type `%s` is exposed in `%s`, but no other module includes `%s`'s interface. Keep representation details private or make the type opaque until callers need it.\n", type->name, type->path, type->module);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->include_count; i++)
    {
        const struct include_record *include;

        include = &map->includes[i];
        if(include->is_local && p101_module_map_module_has_direct_include(env, map, include->target, include->from_module) && p101_strcmp(env, include->from_module, include->target) != 0)
        {
            p101_fprintf(env, err, stream, "- `%s` and `%s` include each other. That direct cycle is a design smell; introduce a smaller shared interface.\n", include->from_module, include->target);
            wrote = true;
        }

        if(include->is_local && !p101_module_map_include_target_exists(env, map, include->target))
        {
            p101_fprintf(env, err, stream, "- `%s` includes local header `%s`, but no scanned module named `%s` was found. Check for a stale include or add the missing module to the scan path.\n", include->path, include->target, include->target);
            wrote = true;
        }

        if(!include->is_local && p101_module_map_is_platform_specific_include(env, include->target))
        {
            p101_fprintf(env, err, stream, "- `%s` includes platform-specific header `<%s>`. Put that behind a p101 wrapper or a clearly named portability boundary.\n", include->path, include->target);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;

        module = &map->modules[i];
        if(module->creates_error_object && !module->destroys_error_object)
        {
            p101_fprintf(env, err, stream, "- `%s` creates a `p101_error`, but no matching `p101_error_destroy` was detected in that module. Keep ownership local unless the API explicitly transfers it.\n", module->name);
            wrote = true;
        }

        if(module->creates_env_object && !module->destroys_env_object)
        {
            p101_fprintf(env, err, stream, "- `%s` creates a `p101_env`, but no matching `p101_env_destroy` was detected in that module. The module that creates an environment should normally destroy it.\n", module->name);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->call_count; i++)
    {
        const struct call_record *call;
        const char               *library;

        call    = &map->calls[i];
        library = p101_module_map_raw_call_library(env, call->name);
        if(library != NULL)
        {
            p101_fprintf(env, err, stream, "- `%s` calls raw `%s` at `%s:%zu`. Prefer the p101 wrapper from `%s`, or add a wrapper if this is an intentional portability boundary.\n", call->module, call->name, call->path, call->line, library);
            wrote = true;
        }
    }

    if(!wrote)
    {
        p101_fputs(env, err, "No obvious module-structure issues found by the v1 heuristics.\n", stream);
    }
}

void p101_module_map_write_functions_for_module(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map, const char *module_name)
{
    bool wrote;

    P101_TRACE(env);
    wrote = false;

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;

        function = &map->functions[i];
        if(p101_strcmp(env, function->module, module_name) == 0)
        {
            const char *visibility;

            if(function->is_header_declaration)
            {
                visibility = "header";
            }
            else if(function->is_static)
            {
                visibility = "private";
            }
            else
            {
                visibility = "public";
            }

            p101_fprintf(env, err, stream, "- `%s` — %s (%s:%zu)\n", function->name, visibility, function->path, function->line);
            wrote = true;
        }
    }

    if(!wrote)
    {
        p101_fputs(env, err, "- No functions detected.\n", stream);
    }

    p101_fputs(env, err, "\n", stream);
}
