#include "report.h"
#include "model.h"
#include "model_query.h"
#include "strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdarg.h>
#include <stdio.h>

enum
{
    ASCII_CONTROL_LIMIT = 32
};

static bool p101_module_map_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target);
static bool p101_module_map_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name);
static bool p101_module_map_is_utility_module(const struct p101_env *env, const char *module_name);
static void p101_module_map_write_finding(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, bool *first_json, size_t *finding_count, const char *id, const char *path, size_t line, const char *format, ...)
    P101_ATTR_PRINTF(10, 11);
static void p101_module_map_write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);

static bool p101_module_map_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    for(size_t i = 0U; i < map->module_count; i++)
    {
        if(p101_strcmp(env, map->modules[i].name, module_name) == 0)
        {
            return map->modules[i].source_count > 0U;
        }
    }
    return false;
}

static bool p101_module_map_is_utility_module(const struct p101_env *env, const char *module_name)
{
    if(p101_strcmp(env, module_name, "util") == 0)
    {
        return true;
    }
    return p101_strcmp(env, module_name, "utils") == 0;
}

static bool p101_module_map_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target)
{
    FILE *stream;
    bool  ret_val;
    char  line[MAX_LINE];

    ret_val = true;
    stream  = NULL;

    if(p101_strcmp(env, from_module, target) == 0)
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

    while(p101_error_has_no_error(err) && p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        char       *left;
        char       *right;
        char       *arrow;
        const char *found;

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

        if(p101_strcmp(env, left, from_module) == 0 && p101_strcmp(env, right, target) == 0)
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
        return p101_module_map_write_findings(env, err, stream, args, map);
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

        module = &map->modules[i];
        if(module->source_count > 0U && module->header_count == 0U && p101_strcmp(env, module->name, "main") != 0)
        {
            p101_module_map_write_finding(env, err, stream, args, &first_json, &finding_count, "P101-MOD-001", module->name, 0U, "`%s` has source code but no matching header. If other modules should use it, create a small public interface.", module->name);
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

        if(!args->library_mode && p101_strcmp(env, module->name, "main") == 0 && module->function_count > 3U)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          stream,
                                          args,
                                          &first_json,
                                          &finding_count,
                                          "P101-MOD-004",
                                          "main.c",
                                          0U,
                                          "`main` has %zu functions. Students usually do better when `main.c` only wires argument parsing, setup, and top-level control flow.",
                                          module->function_count);
            wrote = true;
        }

        if(!args->library_mode && p101_module_map_is_utility_module(env, module->name) && module->function_count > 4U)
        {
            p101_module_map_write_finding(env, err, stream, args, &first_json, &finding_count, "P101-MOD-005", module->name, 0U, "`%s` looks like a utility dumping ground. Try naming modules after responsibilities instead.", module->name);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;

        function = &map->functions[i];
        if(!args->library_mode && !function->is_header_declaration && !function->is_static && !p101_module_map_function_has_header_declaration(env, map, function) && !p101_module_map_function_used_outside_module(env, map, function) &&
           p101_strcmp(env, function->name, "main") != 0)
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

        if(function->is_header_declaration && (!args->library_mode || p101_module_map_module_has_source(env, map, function->module)) && !p101_module_map_function_has_non_static_definition(env, map, function))
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

        if(!args->library_mode && function->is_header_declaration && !p101_module_map_module_used_outside_module(env, map, function->module) && p101_strcmp(env, function->module, "main") != 0)
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

        macro = &map->macros[i];
        if(!p101_module_map_module_used_outside_module(env, map, macro->module) && !p101_module_map_symbol_used_outside_module(env, map, macro->module, macro->name))
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

        type = &map->types[i];
        if(!p101_module_map_module_used_outside_module(env, map, type->module))
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

        include = &map->includes[i];
        if(include->is_local && p101_module_map_module_has_direct_include(env, map, include->target, include->from_module) && p101_strcmp(env, include->from_module, include->target) != 0)
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

        if(include->is_local && !p101_module_map_include_target_exists(env, map, include->target))
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

        if(include->is_local && !p101_module_map_layer_allows_include(env, err, args, include->from_module, include->target))
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
    char    message[MAX_LINE];
    va_list arguments;

    P101_TRACE_SCOPE(env);
    va_start(arguments, format);
    (void)p101_vsnprintf(env, err, message, sizeof(message), format, arguments);
    va_end(arguments);
    if(p101_error_has_error(err))
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
        p101_fputs(env, err, ",\"evidence\":{\"fact_schema\":\"P101FACT-v2\"}}", stream);
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
    P101_TRACE_SCOPE(env);
    p101_fputc(env, err, '"', stream);
    for(const unsigned char *cursor = (const unsigned char *)text; cursor != NULL && *cursor != '\0'; cursor++)
    {
        switch(*cursor)
        {
            case '"':
                p101_fputs(env, err, "\\\"", stream);
                break;
            case '\\':
                p101_fputs(env, err, "\\\\", stream);
                break;
            case '\n':
                p101_fputs(env, err, "\\n", stream);
                break;
            case '\r':
                p101_fputs(env, err, "\\r", stream);
                break;
            case '\t':
                p101_fputs(env, err, "\\t", stream);
                break;
            default:
                if(*cursor < ASCII_CONTROL_LIMIT)
                {
                    p101_fprintf(env, err, stream, "\\u%04x", (unsigned)*cursor);
                }
                else
                {
                    p101_fputc(env, err, *cursor, stream);
                }
                break;
        }
    }
    p101_fputc(env, err, '"', stream);
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
    bool wrote;

    P101_TRACE_SCOPE(env);
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
