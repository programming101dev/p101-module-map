#include "fact_loader.h"
#include "constants.h"
#include "errors.h"
#include "model_mutation.h"
#include "model_notes.h"
#include "strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_stdio.h>
#include <p101_posix/p101_unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

static void                p101_module_map_append_checked(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *text);
static void                p101_module_map_append_char_checked(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, char ch);
static void                p101_module_map_append_shell_quoted(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *text);
static void                p101_module_map_append_cflag(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *flag);
static void                p101_module_map_append_include_roots(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *path);
static const char         *p101_module_map_choose_fact_tool(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static bool                p101_module_map_executable_exists(const struct p101_env *env, struct p101_error *err, const char *path);
static void                p101_module_map_build_fact_command(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const struct arguments *args);
static size_t              p101_module_map_split_fact_line(const struct p101_env *env, char *line, char *fields[], size_t field_count);
static void                p101_module_map_unescape_fact_field(const struct p101_env *env, char *field);
static bool                p101_module_map_fact_bool(const struct p101_env *env, const char *text);
static char               *p101_module_map_find_char(char *text, char ch);
static struct source_file *p101_module_map_find_fact_file(const struct p101_env *env, struct project_map *map, const char *path);
static struct source_file *p101_module_map_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, char *fields[], size_t field_count);
static void                p101_module_map_note_call_semantics(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name);
static void                p101_module_map_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, char *fields[], size_t field_count);

static void p101_module_map_append_checked(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *text)
{
    size_t used;
    size_t extra;

    P101_TRACE(env);
    used  = p101_strlen(env, command);
    extra = p101_strlen(env, text);
    if(used + extra >= command_size)
    {
        P101_ERROR_RAISE_USER(err, "The p101-wrapper-audit command is too long.", ERR_USAGE);
        goto done;
    }
    p101_module_map_copy_string(env, command + used, command_size - used, text);

done:
    return;
}

static void p101_module_map_append_char_checked(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, char ch)
{
    char text[2];

    P101_TRACE(env);
    text[0] = ch;
    text[1] = '\0';
    p101_module_map_append_checked(env, err, command, command_size, text);
}

static void p101_module_map_append_shell_quoted(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *text)
{
    P101_TRACE(env);
    p101_module_map_append_char_checked(env, err, command, command_size, '\'');
    for(size_t i = 0; text[i] != '\0' && p101_error_has_no_error(err); i++)
    {
        if(text[i] == '\'')
        {
            p101_module_map_append_checked(env, err, command, command_size, "'\\''");
        }
        else
        {
            p101_module_map_append_char_checked(env, err, command, command_size, text[i]);
        }
    }
    p101_module_map_append_char_checked(env, err, command, command_size, '\'');
}

static void p101_module_map_append_cflag(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *flag)
{
    P101_TRACE(env);
    p101_module_map_append_checked(env, err, command, command_size, " --cflag=");
    p101_module_map_append_shell_quoted(env, err, command, command_size, flag);
}

static void p101_module_map_append_include_roots(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *path)
{
    char root[PATH_LEN];
    char include_arg[PATH_LEN + 3U];

    P101_TRACE(env);
    p101_module_map_copy_string(env, root, sizeof(root), path);

    p101_snprintf(env, err, include_arg, sizeof(include_arg), "-I%s", root);
    p101_module_map_append_cflag(env, err, command, command_size, include_arg);
    p101_snprintf(env, err, include_arg, sizeof(include_arg), "-I%s/include", root);
    p101_module_map_append_cflag(env, err, command, command_size, include_arg);
    p101_snprintf(env, err, include_arg, sizeof(include_arg), "-I%s/../include", root);
    p101_module_map_append_cflag(env, err, command, command_size, include_arg);
}

static const char *p101_module_map_choose_fact_tool(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    const char *tool;

    P101_TRACE(env);
    tool = args->fact_tool_path;
    if(tool != NULL && tool[0] != '\0')
    {
        goto done;
    }

    tool = p101_getenv(env, "P101_MODULE_MAP_FACT_TOOL");
    if(tool != NULL && tool[0] != '\0')
    {
        goto done;
    }

    tool = p101_getenv(env, "P101_WRAPPER_AUDIT");
    if(tool != NULL && tool[0] != '\0')
    {
        goto done;
    }

    tool = "../p101-wrapper-audit/p101-wrapper-audit";
    if(p101_module_map_executable_exists(env, err, tool))
    {
        goto done;
    }

    tool = "../../programs/p101-wrapper-audit/p101-wrapper-audit";
    if(p101_module_map_executable_exists(env, err, tool))
    {
        goto done;
    }

    tool = "p101-wrapper-audit";

done:
    return tool;
}

static bool p101_module_map_executable_exists(const struct p101_env *env, struct p101_error *err, const char *path)
{
    bool ret_val;

    P101_TRACE(env);
    ret_val = p101_access(env, err, path, X_OK) == 0;
    if(p101_error_has_error(err))
    {
        p101_error_reset(err);
    }
    return ret_val;
}

static void p101_module_map_build_fact_command(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const struct arguments *args)
{
    const char *tool;

    P101_TRACE(env);
    command[0] = '\0';
    tool       = p101_module_map_choose_fact_tool(env, err, args);
    p101_module_map_append_shell_quoted(env, err, command, command_size, tool);
    p101_module_map_append_checked(env, err, command, command_size, " --emit-module-facts");
    p101_module_map_append_include_roots(env, err, command, command_size, ".");
    p101_module_map_append_include_roots(env, err, command, command_size, "include");

    if(args->path_count == 0U)
    {
        p101_module_map_append_include_roots(env, err, command, command_size, ".");
    }
    else
    {
        for(size_t i = 0; i < args->path_count && p101_error_has_no_error(err); i++)
        {
            p101_module_map_append_include_roots(env, err, command, command_size, args->paths[i]);
        }
    }

    if(args->path_count == 0U)
    {
        p101_module_map_append_char_checked(env, err, command, command_size, ' ');
        p101_module_map_append_shell_quoted(env, err, command, command_size, ".");
    }
    else
    {
        for(size_t i = 0; i < args->path_count && p101_error_has_no_error(err); i++)
        {
            p101_module_map_append_char_checked(env, err, command, command_size, ' ');
            p101_module_map_append_shell_quoted(env, err, command, command_size, args->paths[i]);
        }
    }

    p101_module_map_append_checked(env, err, command, command_size, " 2>&1");
}

static size_t p101_module_map_split_fact_line(const struct p101_env *env, char *line, char *fields[], size_t field_count)
{
    size_t count;
    char  *cursor;

    P101_TRACE(env);
    count  = 0U;
    cursor = line;
    while(count < field_count)
    {
        char *tab;

        fields[count++] = cursor;
        tab             = p101_module_map_find_char(cursor, '\t');
        if(tab == NULL)
        {
            break;
        }
        *tab   = '\0';
        cursor = tab + 1;
    }

    for(size_t i = 0; i < count; i++)
    {
        p101_module_map_unescape_fact_field(env, fields[i]);
    }

    return count;
}

static void p101_module_map_unescape_fact_field(const struct p101_env *env, char *field)
{
    char *read_cursor;
    char *write_cursor;

    P101_TRACE(env);
    read_cursor  = field;
    write_cursor = field;
    while(*read_cursor != '\0')
    {
        if(read_cursor[0] == '\\' && read_cursor[1] != '\0')
        {
            read_cursor++;
            if(*read_cursor == 't')
            {
                *write_cursor++ = '\t';
            }
            else if(*read_cursor == 'n')
            {
                *write_cursor++ = '\n';
            }
            else
            {
                *write_cursor++ = *read_cursor;
            }
            read_cursor++;
        }
        else
        {
            *write_cursor++ = *read_cursor++;
        }
    }
    *write_cursor = '\0';
}

static bool p101_module_map_fact_bool(const struct p101_env *env, const char *text)
{
    bool ret_val;

    P101_TRACE(env);
    ret_val = false;
    if(text != NULL && p101_strcmp(env, text, "0") != 0)
    {
        ret_val = true;
    }
    return ret_val;
}

static char *p101_module_map_find_char(char *text, char ch)
{
    char *ret_val;

    ret_val = NULL;
    while(*text != '\0')
    {
        if(*text == ch)
        {
            ret_val = text;
            break;
        }
        text++;
    }
    return ret_val;
}

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

static struct source_file *p101_module_map_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, char *fields[], size_t field_count)
{
    struct source_file *file;

    P101_TRACE(env);
    file = NULL;
    if(field_count < FACT_BASE_FIELD_COUNT)
    {
        goto done;
    }

    file = p101_module_map_find_fact_file(env, map, fields[FACT_PATH_IDX]);
    if(file == NULL)
    {
        p101_module_map_add_source_file(env, err, map, fields[FACT_PATH_IDX], p101_module_map_fact_bool(env, fields[FACT_IS_HEADER_IDX]));
        file = p101_module_map_find_fact_file(env, map, fields[FACT_PATH_IDX]);
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

static void p101_module_map_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, char *fields[], size_t field_count)
{
    const struct source_file *file;
    size_t                    line;

    P101_TRACE(env);
    if(field_count < FACT_BASE_FIELD_COUNT || p101_strcmp(env, fields[FACT_TAG_IDX], "P101FACT") != 0)
    {
        goto done;
    }

    file = p101_module_map_file_for_fact(env, err, map, fields, field_count);
    if(file == NULL || p101_error_has_error(err))
    {
        goto done;
    }

    line = p101_parse_unsigned_long(env, err, fields[FACT_LINE_IDX], 0U);
    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(p101_strcmp(env, fields[FACT_KIND_IDX], "FILE") == 0)
    {
        goto done;
    }
    if(p101_strcmp(env, fields[FACT_KIND_IDX], "INCLUDE") == 0 && field_count >= FACT_INCLUDE_FIELDS)
    {
        p101_module_map_add_include(env, err, map, file, fields[FACT_VALUE_IDX], line, p101_module_map_fact_bool(env, fields[FACT_FLAG1_IDX]));
    }
    else if(p101_strcmp(env, fields[FACT_KIND_IDX], "MACRO") == 0 && field_count >= FACT_VALUE_FIELDS)
    {
        p101_module_map_add_macro(env, err, map, file, fields[FACT_VALUE_IDX], line);
    }
    else if(p101_strcmp(env, fields[FACT_KIND_IDX], "TYPE") == 0 && field_count >= FACT_VALUE_FIELDS)
    {
        p101_module_map_add_type(env, err, map, file, fields[FACT_VALUE_IDX], line);
    }
    else if(p101_strcmp(env, fields[FACT_KIND_IDX], "FUNCTION") == 0 && field_count >= FACT_FUNCTION_FIELDS)
    {
        p101_module_map_add_function(env, err, map, file, fields[FACT_VALUE_IDX], line, p101_module_map_fact_bool(env, fields[FACT_FLAG1_IDX]), p101_module_map_fact_bool(env, fields[FACT_FLAG2_IDX]));
    }
    else if(p101_strcmp(env, fields[FACT_KIND_IDX], "CALL") == 0 && field_count >= FACT_VALUE_FIELDS)
    {
        p101_module_map_add_call(env, err, map, file, fields[FACT_VALUE_IDX], line);
        p101_module_map_note_call_semantics(env, err, map, file, fields[FACT_VALUE_IDX]);
    }
    else if(p101_strcmp(env, fields[FACT_KIND_IDX], "NOTE") == 0 && field_count >= FACT_VALUE_FIELDS)
    {
        if(p101_strcmp(env, fields[FACT_VALUE_IDX], "ERROR_USE") == 0)
        {
            p101_module_map_note_error_use(env, err, map, file);
        }
        else if(p101_strcmp(env, fields[FACT_VALUE_IDX], "ERROR_CHECK") == 0)
        {
            p101_module_map_note_error_use(env, err, map, file);
            p101_module_map_note_error_check(env, err, map, file);
        }
    }

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
        char  *fields[MAX_FACT_FIELDS];
        char  *newline;
        size_t field_count;

        newline = p101_module_map_find_char(line, '\n');
        if(newline != NULL)
        {
            *newline = '\0';
        }

        if(p101_strncmp(env, line, "P101FACT\t", FACT_PREFIX_LEN) != 0)
        {
            continue;
        }

        field_count = p101_module_map_split_fact_line(env, line, fields, MAX_FACT_FIELDS);
        p101_module_map_apply_fact(env, err, map, fields, field_count);
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
