#include "report.h"
#include "model.h"
#include "model_query.h"
#include "strings.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_record/record.h>
#include <stdarg.h>
#include <stdio.h>

static bool p101_module_map_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target);
static bool p101_module_map_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name);
static bool p101_module_map_module_has_unmatched_public_definition(const struct p101_env *env, const struct project_map *map, const char *module_name);
static bool p101_module_map_module_contains_entrypoint(const struct p101_env *env, const struct project_map *map, const char *module_name);
static void p101_module_map_write_finding(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, bool *first_json, size_t *finding_count, const char *id, const char *path, size_t line, const char *format, ...)
    P101_ATTR_PRINTF(10, 11);
static void p101_module_map_write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);

static bool p101_module_map_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    int  p101_call_result_1;
    bool has_source;

    has_source = false;
    for(size_t i = 0U; i < map->module_count; i++)
    {
        p101_call_result_1 = p101_strcmp(env, map->modules[i].name, module_name);
        if(p101_call_result_1 == 0)
        {
            has_source = map->modules[i].source_count > 0U;
            break;
        }
    }
    return has_source;
}

static bool p101_module_map_module_has_unmatched_public_definition(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    int  p101_expression_result_8;
    int  p101_expression_result_9;
    int  p101_expression_result_10;
    int  p101_expression_result_11;
    int  p101_call_result_12;
    int  p101_call_result_13;
    bool p101_call_result_14;
    bool has_unmatched;

    has_unmatched = false;
    for(size_t i = 0U; i < map->function_count; i++)
    {
        const struct function_record *function;

        function                  = &map->functions[i];
        p101_expression_result_11 = 0;
        if(!function->is_header_declaration)
        {
            if(!function->is_static)
            {
                p101_expression_result_11 = 1;
            }
        }
        p101_expression_result_10 = 0;
        if(p101_expression_result_11)
        {
            p101_call_result_12 = p101_strcmp(env, function->module, module_name);
            if(p101_call_result_12 == 0)
            {
                p101_expression_result_10 = 1;
            }
        }
        p101_expression_result_9 = 0;
        if(p101_expression_result_10)
        {
            p101_call_result_13 = p101_strcmp(env, function->usr, "c:@F@main");
            if(p101_call_result_13 != 0)
            {
                p101_expression_result_9 = 1;
            }
        }
        p101_expression_result_8 = 0;
        if(p101_expression_result_9)
        {
            p101_call_result_14 = p101_module_map_function_has_header_declaration(env, map, function);
            if(!p101_call_result_14)
            {
                p101_expression_result_8 = 1;
            }
        }
        if(p101_expression_result_8)
        {
            has_unmatched = true;
            break;
        }
    }
    return has_unmatched;
}

static bool p101_module_map_module_contains_entrypoint(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    int  p101_expression_result_15;
    int  p101_call_result_16;
    int  p101_call_result_17;
    bool contains_entrypoint;

    contains_entrypoint = false;
    for(size_t index = 0U; index < map->function_count; index++)
    {
        p101_call_result_16       = p101_strcmp(env, map->functions[index].module, module_name);
        p101_expression_result_15 = 0;
        if(p101_call_result_16 == 0)
        {
            p101_call_result_17 = p101_strcmp(env, map->functions[index].usr, "c:@F@main");
            if(p101_call_result_17 == 0)
            {
                p101_expression_result_15 = 1;
            }
        }
        if(p101_expression_result_15)
        {
            contains_entrypoint = true;
            break;
        }
    }
    return contains_entrypoint;
}

static bool p101_module_map_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target)
{
    int   p101_expression_result_18;
    int   p101_call_result_19;
    int   p101_call_result_20;
    int   p101_call_result_2;
    FILE *stream;
    bool  ret_val;
    char  line[MAX_LINE];
    bool  no_error;
    char *line_result;

    ret_val = true;
    stream  = NULL;

    p101_call_result_2 = p101_strcmp(env, from_module, target);
    if(p101_call_result_2 == 0)
    {
        goto done;
    }

    if(args->layer_config_path == NULL)
    {
        goto done;
    }

    ret_val = false;
    stream  = p101_fopen(env, err, args->layer_config_path, "r");

    if(stream == NULL)
    {
        goto done;
    }

    for(;;)
    {
        char       *left;
        char       *right;
        char       *arrow;
        const char *found;

        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
        line_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(line_result == NULL)
        {
            break;
        }
        p101_module_map_trim_right(env, line);
        left = p101_module_map_trim_left(env, line);

        if(left[0] == '\0' || left[0] == '#')
        {
            continue;
        }

        found = p101_strstr(env, left, "->");

        if(found == NULL)
        {
            continue;
        }

        arrow    = &left[found - left];
        arrow[0] = '\0';
        p101_module_map_trim_right(env, left);
        right = p101_module_map_trim_left(env, arrow + 2);
        p101_module_map_trim_right(env, right);

        p101_call_result_19       = p101_strcmp(env, left, from_module);
        p101_expression_result_18 = 0;
        if(p101_call_result_19 == 0)
        {
            p101_call_result_20 = p101_strcmp(env, right, target);
            if(p101_call_result_20 == 0)
            {
                p101_expression_result_18 = 1;
            }
        }
        if(p101_expression_result_18)
        {
            ret_val = true;
            break;
        }
    }

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }

    return ret_val;
}

bool p101_module_map_write_report(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map)
{
    bool has_findings;

    P101_TRACE_SCOPE(env);
    if(args->json)
    {
        has_findings = p101_module_map_write_findings(env, err, stream, args, map);
        goto done;
    }
    p101_fputs(env, err, "# p101 module map\n\n", stream);
    p101_fputs(env, err, "> Parser note: this report consumes Clang AST facts from `p101-wrapper-audit`; the module design checks are still teaching heuristics, not proof obligations.\n\n", stream);
    if(args->library_mode)
    {
        p101_fputs(env, err, "> Library mode: public API use requires external consumers, so this report omits closed-world unused-public-symbol checks.\n\n", stream);
    }
    p101_fprintf(env, err, stream, "Files scanned: `%zu`\n\n", map->file_count);
    p101_fprintf(env, err, stream, "Modules found: `%zu`\n\n", map->module_count);
    p101_fprintf(env, err, stream, "Functions found: `%zu`\n\n", map->function_count);
    if(map->calls_dropped > 0U)
    {
        p101_fprintf(env, err, stream, "Call-like tokens dropped after cap: `%zu`\n\n", map->calls_dropped);
    }
    p101_module_map_write_modules(env, err, stream, map);
    p101_module_map_write_include_graph(env, err, stream, map);
    has_findings = p101_module_map_write_findings(env, err, stream, args, map);

done:
    return has_findings;
}

void p101_module_map_write_modules(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map)
{
    P101_TRACE_SCOPE(env);
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
    P101_TRACE_SCOPE(env);
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

bool p101_module_map_write_findings(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map)
{
    int    p101_expression_result_21;
    int    p101_expression_result_22;
    int    p101_expression_result_23;
    bool   p101_call_result_24;
    bool   p101_call_result_25;
    int    p101_expression_result_26;
    int    p101_expression_result_27;
    bool   p101_call_result_28;
    int    p101_expression_result_29;
    int    p101_expression_result_30;
    int    p101_expression_result_31;
    int    p101_expression_result_32;
    int    p101_expression_result_33;
    bool   p101_call_result_34;
    bool   p101_call_result_35;
    int    p101_call_result_36;
    int    p101_expression_result_37;
    int    p101_expression_result_38;
    int    p101_expression_result_39;
    bool   p101_call_result_40;
    bool   p101_call_result_41;
    int    p101_expression_result_42;
    int    p101_expression_result_43;
    int    p101_expression_result_44;
    bool   p101_call_result_45;
    bool   p101_call_result_46;
    int    p101_expression_result_47;
    bool   p101_call_result_48;
    bool   p101_call_result_49;
    int    p101_expression_result_50;
    int    p101_expression_result_51;
    bool   p101_call_result_52;
    int    p101_call_result_53;
    int    p101_expression_result_54;
    bool   p101_call_result_55;
    int    p101_expression_result_56;
    bool   p101_call_result_57;
    bool   p101_call_result_3;
    bool   wrote;
    bool   first_json;
    size_t finding_count;

    P101_TRACE_SCOPE(env);
    wrote         = false;
    first_json    = true;
    finding_count = 0U;
    if(args->json)
    {
        p101_fputs(env, err, "{\"schema\":\"p101-module-map-findings-v1\",\"findings\":[", stream);
    }
    else
    {
        p101_fputs(env, err, "## Teaching notes\n\n", stream);
    }

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;

        module                    = &map->modules[i];
        p101_expression_result_23 = 0;
        if(module->source_count > 0U)
        {
            if(module->header_count == 0U)
            {
                p101_expression_result_23 = 1;
            }
        }
        p101_expression_result_22 = 0;
        if(p101_expression_result_23)
        {
            p101_call_result_24 = p101_module_map_module_contains_entrypoint(env, map, module->name);
            if(!p101_call_result_24)
            {
                p101_expression_result_22 = 1;
            }
        }
        p101_expression_result_21 = 0;
        if(p101_expression_result_22)
        {
            p101_call_result_25 = p101_module_map_module_has_unmatched_public_definition(env, map, module->name);
            if(p101_call_result_25)
            {
                p101_expression_result_21 = 1;
            }
        }
        if(p101_expression_result_21)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-001",
                                          module->name,
                                          0U,
                                          "`%s` exposes a function without any scanned header declaration. Add the function to the appropriate interface or make it `static`.",
                                          module->name);
            wrote = true;
        }

        if(!args->library_mode && module->function_count > args->max_functions)
        {
            p101_module_map_write_finding(env, err, stream, args, &first_json, &finding_count, "P101-MOD-002", module->name, 0U, "`%s` has %zu functions. Consider splitting one responsibility into another module.", module->name, module->function_count);
            wrote = true;
        }

        if(!args->library_mode && module->public_function_count > args->max_public)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-003",
                                          module->name,
                                          0U,
                                          "`%s` exposes %zu non-static functions. Consider making helpers `static` or narrowing the public API.",
                                          module->name,
                                          module->public_function_count);
            wrote = true;
        }

        p101_expression_result_27 = 0;
        if(!args->library_mode)
        {
            p101_call_result_28 = p101_module_map_module_contains_entrypoint(env, map, module->name);
            if(p101_call_result_28)
            {
                p101_expression_result_27 = 1;
            }
        }
        p101_expression_result_26 = 0;
        if(p101_expression_result_27)
        {
            if(module->function_count > 3U)
            {
                p101_expression_result_26 = 1;
            }
        }
        if(p101_expression_result_26)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-004",
                                          module->source_path,
                                          0U,
                                          "`main` has %zu functions. Students usually do better when `main.c` only wires argument parsing, setup, and top-level control flow.",
                                          module->function_count);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;

        function                  = &map->functions[i];
        p101_expression_result_33 = 0;
        if(!args->library_mode)
        {
            if(!function->is_header_declaration)
            {
                p101_expression_result_33 = 1;
            }
        }
        p101_expression_result_32 = 0;
        if(p101_expression_result_33)
        {
            if(!function->is_static)
            {
                p101_expression_result_32 = 1;
            }
        }
        p101_expression_result_31 = 0;
        if(p101_expression_result_32)
        {
            p101_call_result_34 = p101_module_map_function_has_header_declaration(env, map, function);
            if(!p101_call_result_34)
            {
                p101_expression_result_31 = 1;
            }
        }
        p101_expression_result_30 = 0;
        if(p101_expression_result_31)
        {
            p101_call_result_35 = p101_module_map_function_used_outside_module(env, map, function);
            if(!p101_call_result_35)
            {
                p101_expression_result_30 = 1;
            }
        }
        p101_expression_result_29 = 0;
        if(p101_expression_result_30)
        {
            p101_call_result_36 = p101_strcmp(env, function->usr, "c:@F@main");
            if(p101_call_result_36 != 0)
            {
                p101_expression_result_29 = 1;
            }
        }
        if(p101_expression_result_29)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-006",
                                          function->path,
                                          function->line,
                                          "`%s` is non-static but does not appear to be part of a used module interface. Prefer `static`; only leave it public when another module must call it through the header.",
                                          function->name);
            wrote = true;
        }

        p101_expression_result_38 = 0;
        if(function->is_header_declaration)
        {
            if(!args->library_mode)
            {
                p101_expression_result_39 = 1;
            }
            else
            {
                p101_call_result_40 = p101_module_map_module_has_source(env, map, function->module);
                if(p101_call_result_40)
                {
                    p101_expression_result_39 = 1;
                }
                else
                {
                    p101_expression_result_39 = 0;
                }
            }
            if(p101_expression_result_39)
            {
                p101_expression_result_38 = 1;
            }
        }
        p101_expression_result_37 = 0;
        if(p101_expression_result_38)
        {
            p101_call_result_41 = p101_module_map_function_has_non_static_definition(env, map, function);
            if(!p101_call_result_41)
            {
                p101_expression_result_37 = 1;
            }
        }
        if(p101_expression_result_37)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-007",
                                          function->path,
                                          function->line,
                                          "`%s` is declared here, but no matching non-static definition was found. Remove the declaration or make the implementation match the public API.",
                                          function->name);
            wrote = true;
        }

        p101_expression_result_44 = 0;
        if(!args->library_mode)
        {
            if(function->is_header_declaration)
            {
                p101_expression_result_44 = 1;
            }
        }
        p101_expression_result_43 = 0;
        if(p101_expression_result_44)
        {
            p101_call_result_45 = p101_module_map_module_used_outside_module(env, map, function->module);
            if(!p101_call_result_45)
            {
                p101_expression_result_43 = 1;
            }
        }
        p101_expression_result_42 = 0;
        if(p101_expression_result_43)
        {
            p101_call_result_46 = p101_module_map_module_contains_entrypoint(env, map, function->module);
            if(!p101_call_result_46)
            {
                p101_expression_result_42 = 1;
            }
        }
        if(p101_expression_result_42)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-008",
                                          function->path,
                                          function->line,
                                          "`%s` is declared here, but no other module includes `%s`'s interface. Keep helpers `static` and remove header declarations until another file needs them.",
                                          function->name,
                                          function->module);
            wrote = true;
        }
    }

    for(size_t i = 0; !args->library_mode && i < map->macro_count; i++)
    {
        const struct macro_record *macro;

        macro                     = &map->macros[i];
        p101_call_result_48       = p101_module_map_module_used_outside_module(env, map, macro->module);
        p101_expression_result_47 = 0;
        if(!p101_call_result_48)
        {
            p101_call_result_49 = p101_module_map_symbol_used_outside_module(env, map, macro->module, macro->name);
            if(!p101_call_result_49)
            {
                p101_expression_result_47 = 1;
            }
        }
        if(p101_expression_result_47)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-009",
                                          macro->path,
                                          macro->line,
                                          "Macro `%s` is exposed, but the module interface does not appear to be used outside `%s`. Prefer a private enum/constant in the `.c` file until another module needs it.",
                                          macro->name,
                                          macro->module);
            wrote = true;
        }
    }

    for(size_t i = 0; !args->library_mode && i < map->type_count; i++)
    {
        const struct type_record *type;

        type               = &map->types[i];
        p101_call_result_3 = p101_module_map_module_used_outside_module(env, map, type->module);
        if(!p101_call_result_3)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-010",
                                          type->path,
                                          type->line,
                                          "Type `%s` is exposed, but no other module includes `%s`'s interface. Keep representation details private or make the type opaque until callers need it.",
                                          type->name,
                                          type->module);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->include_count; i++)
    {
        const struct include_record *include;

        include                   = &map->includes[i];
        p101_expression_result_51 = 0;
        if(include->is_local)
        {
            p101_call_result_52 = p101_module_map_module_has_direct_include(env, map, include->target, include->from_module);
            if(p101_call_result_52)
            {
                p101_expression_result_51 = 1;
            }
        }
        p101_expression_result_50 = 0;
        if(p101_expression_result_51)
        {
            p101_call_result_53 = p101_strcmp(env, include->from_module, include->target);
            if(p101_call_result_53 != 0)
            {
                p101_expression_result_50 = 1;
            }
        }
        if(p101_expression_result_50)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-011",
                                          include->path,
                                          include->line,
                                          "`%s` and `%s` include each other. That direct cycle is a design smell; introduce a smaller shared interface.",
                                          include->from_module,
                                          include->target);
            wrote = true;
        }

        p101_expression_result_54 = 0;
        if(include->is_local)
        {
            p101_call_result_55 = p101_module_map_include_target_exists(env, map, include->target);
            if(!p101_call_result_55)
            {
                p101_expression_result_54 = 1;
            }
        }
        if(p101_expression_result_54)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-012",
                                          include->path,
                                          include->line,
                                          "This file includes local header `%s`, but no scanned module named `%s` was found. Check for a stale include or add the missing module to the scan path.",
                                          include->target,
                                          include->target);
            wrote = true;
        }

        p101_expression_result_56 = 0;
        if(include->is_local)
        {
            p101_call_result_57 = p101_module_map_layer_allows_include(env, err, args, include->from_module, include->target);
            if(!p101_call_result_57)
            {
                p101_expression_result_56 = 1;
            }
        }
        if(p101_expression_result_56)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-013",
                                          include->path,
                                          include->line,
                                          "`%s` includes `%s`, but that edge is not allowed by `%s`. Add `%s -> %s` only if this dependency is intentional.",
                                          include->from_module,
                                          include->target,
                                          args->layer_config_path,
                                          include->from_module,
                                          include->target);
            wrote = true;
        }
    }

    if(args->json)
    {
        p101_fprintf(env, err, stream, "],\"summary\":{\"files_scanned\":%zu,\"modules\":%zu,\"functions\":%zu,\"findings\":%zu}}\n", map->file_count, map->module_count, map->function_count, finding_count);
    }
    else if(!wrote)
    {
        p101_fputs(env, err, "No obvious module-structure issues found by the current heuristics.\n", stream);
    }

    return wrote;
}

static void p101_module_map_write_finding(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, bool *first_json, size_t *finding_count, const char *id, const char *path, size_t line, const char *format, ...)
{
    int     p101_call_result_4;
    bool    p101_call_result_5;
    char    message[MAX_LINE];
    va_list arguments;

    P101_TRACE_SCOPE(env);
    va_start(arguments, format);
    p101_call_result_4 = p101_vsnprintf(env, err, message, sizeof(message), format, arguments);
    (void)p101_call_result_4;
    va_end(arguments);
    p101_call_result_5 = p101_error_has_error(err);
    if(p101_call_result_5)
    {
        goto done;
    }

    (*finding_count)++;
    if(args->json)
    {
        if(!*first_json)
        {
            p101_fputc(env, err, ',', stream);
        }
        *first_json = false;
        p101_fputs(env, err, "{\"id\":", stream);
        p101_module_map_write_json_string(env, err, stream, id);
        p101_fputs(env, err, ",\"severity\":\"warning\",\"location\":{\"path\":", stream);
        p101_module_map_write_json_string(env, err, stream, path);
        p101_fprintf(env, err, stream, ",\"line\":%zu},\"message\":", line);
        p101_module_map_write_json_string(env, err, stream, message);
        p101_fputs(env, err, ",\"evidence\":{\"fact_schema\":\"P101FACT-v6\"}}", stream);
    }
    else
    {
        p101_fprintf(env, err, stream, "- %s: %s", id, message);
        if(path != NULL && path[0] != '\0')
        {
            p101_fprintf(env, err, stream, " (`%s:%zu`)", path, line);
        }
        p101_fputc(env, err, '\n', stream);
    }

done:
    return;
}

static void p101_module_map_write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    int p101_call_result_6;
    P101_TRACE_SCOPE(env);
    p101_call_result_6 = p101_record_write_json_string(stream, text == NULL ? "" : text);
    if(p101_call_result_6 != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}

#ifdef P101_MODULE_MAP_TESTING
bool p101_module_map_test_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    return p101_module_map_module_has_source(env, map, module_name);
}

bool p101_module_map_test_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target)
{
    return p101_module_map_layer_allows_include(env, err, args, from_module, target);
}

void p101_module_map_test_write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    p101_module_map_write_json_string(env, err, stream, text);
}

void p101_module_map_test_write_finding(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, bool *first_json, size_t *finding_count, const char *path)
{
    p101_module_map_write_finding(env, err, stream, args, first_json, finding_count, "P101-TEST", path, 1U, "message");
}
#endif

void p101_module_map_write_functions_for_module(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map, const char *module_name)
{
    int  p101_call_result_7;
    bool wrote;

    P101_TRACE_SCOPE(env);
    wrote = false;

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;

        function           = &map->functions[i];
        p101_call_result_7 = p101_strcmp(env, function->module, module_name);
        if(p101_call_result_7 == 0)
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
