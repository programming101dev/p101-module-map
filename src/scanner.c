#include "scanner.h"
#include "constants.h"
#include "model.h"
#include "model_mutation.h"
#include "model_notes.h"
#include "parse.h"
#include "strings.h"
#include <dirent.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_dirent.h>
#include <p101_posix/sys/p101_stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static void p101_module_map_strip_comments(char *line, bool *in_block_comment);
static void p101_module_map_scan_call_tokens(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *line, size_t line_number);
static void p101_module_map_scan_directory(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path) /* NOLINT(misc-no-recursion) */;
static void p101_module_map_scan_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path);

static void p101_module_map_strip_comments(char *line, bool *in_block_comment)
{
    char *read_cursor;
    char *write_cursor;
    bool  in_string;
    bool  in_character;
    bool  escaped;

    read_cursor  = line;
    write_cursor = line;
    in_string    = false;
    in_character = false;
    escaped      = false;

    while(*read_cursor != '\0')
    {
        if(*in_block_comment)
        {
            if(read_cursor[0] == '*' && read_cursor[1] == '/')
            {
                *in_block_comment = false;
                read_cursor += 2;
            }
            else
            {
                read_cursor++;
            }
            continue;
        }

        if(!in_string && !in_character)
        {
            if(read_cursor[0] == '/' && read_cursor[1] == '*')
            {
                *in_block_comment = true;
                read_cursor += 2;
                continue;
            }

            if(read_cursor[0] == '/' && read_cursor[1] == '/')
            {
                break;
            }
        }

        *write_cursor = *read_cursor;

        if(escaped)
        {
            escaped = false;
        }
        else if(in_string)
        {
            if(*read_cursor == '\\')
            {
                escaped = true;
            }
            else if(*read_cursor == '"')
            {
                in_string = false;
            }
        }
        else if(in_character)
        {
            if(*read_cursor == '\\')
            {
                escaped = true;
            }
            else if(*read_cursor == '\'')
            {
                in_character = false;
            }
        }
        else if(*read_cursor == '"')
        {
            in_string = true;
        }
        else if(*read_cursor == '\'')
        {
            in_character = true;
        }

        read_cursor++;
        write_cursor++;
    }

    *write_cursor = '\0';
}

static void p101_module_map_scan_call_tokens(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *line, size_t line_number)
{
    const char *cursor;

    cursor = line;
    while((cursor = p101_strchr(env, cursor, '(')) != NULL && p101_error_has_no_error(err))
    {
        const char *end;
        const char *start;
        char        name[MAX_NAME];
        size_t      len;

        end = cursor;
        while(end > line && (end[-1] == ' ' || end[-1] == '\t'))
        {
            end--;
        }

        start = end;
        while(start > line && (p101_isalnum(env, (unsigned char)start[-1]) || start[-1] == '_'))
        {
            start--;
        }

        len = (size_t)(end - start);
        if(len > 0U && len < sizeof(name))
        {
            for(size_t i = 0; i < len; i++)
            {
                name[i] = start[i];
            }
            name[len] = '\0';

            if(!p101_module_map_looks_like_control_keyword(env, name) && p101_strcmp(env, name, "defined") != 0)
            {
                p101_module_map_add_call(env, err, map, file, name, line_number);
            }
        }

        cursor++;
    }
}

void p101_module_map_scan_path(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path)    // NOLINT(misc-no-recursion)
{
    struct stat info;

    P101_TRACE(env);
    if(p101_lstat(env, err, path, &info) != 0)
    {
        p101_error_reset(err);
        goto done;
    }

    if(S_ISLNK(info.st_mode))
    {
        goto done;
    }

    if(S_ISDIR(info.st_mode))
    {
        p101_module_map_scan_directory(env, err, map, path);
    }
    else if(S_ISREG(info.st_mode))
    {
        p101_module_map_scan_file(env, err, map, path);
    }

done:
    return;
}

static void p101_module_map_scan_directory(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path)    // NOLINT(misc-no-recursion)
{
    DIR           *dir;
    struct dirent *entry;

    P101_TRACE(env);
    dir = p101_opendir(env, err, path);
    if(dir == NULL)
    {
        p101_error_reset(err);
        goto done;
    }

    for(;;)
    {
        char child[PATH_LEN];

        entry = p101_readdir(env, err, dir);
        if(entry == NULL)
        {
            if(p101_error_is_errno(err, 0))
            {
                p101_error_reset(err);
            }
            break;
        }

        if(p101_error_has_error(err))
        {
            p101_error_reset(err);
            break;
        }

        if(p101_strcmp(env, entry->d_name, ".") == 0 || p101_strcmp(env, entry->d_name, "..") == 0 || p101_module_map_is_skipped_dir(env, entry->d_name))
        {
            continue;
        }

        p101_snprintf(env, err, child, sizeof(child), "%s/%s", path, entry->d_name);
        p101_module_map_scan_path(env, err, map, child);
    }

done:
    if(dir != NULL)
    {
        p101_closedir(env, err, dir);
    }
}

static void p101_module_map_scan_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path)
{
    P101_TRACE(env);
    if(p101_module_map_is_source_suffix(env, path))
    {
        p101_module_map_add_source_file(env, err, map, path, false);
    }
    else if(p101_module_map_is_header_suffix(env, path))
    {
        p101_module_map_add_source_file(env, err, map, path, true);
    }
}

void p101_module_map_parse_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    FILE  *stream;
    char   line[MAX_LINE];
    char   signature[MAX_SIGNATURE];
    size_t line_number;
    bool   in_block_comment;

    P101_TRACE(env);
    stream           = p101_fopen(env, err, file->path, "r");
    line_number      = 0U;
    in_block_comment = false;
    signature[0]     = '\0';

    if(stream == NULL)
    {
        p101_error_reset(err);
        goto done;
    }

    while(p101_fgets(env, err, line, sizeof(line), stream) != NULL && p101_error_has_no_error(err))
    {
        char  include_target[MAX_NAME];
        char  function_name[MAX_NAME];
        char  public_name[MAX_NAME];
        char *trimmed;
        bool  is_local;
        bool  is_static;

        line_number++;
        p101_module_map_strip_comments(line, &in_block_comment);
        trimmed = p101_module_map_trim_left(env, line);
        p101_module_map_trim_right(env, trimmed);

        if(!file->is_header && (p101_strstr(env, trimmed, "struct p101_error") != NULL || p101_strstr(env, trimmed, "P101_ERROR_") != NULL || p101_strstr(env, trimmed, "p101_error_") != NULL || p101_strstr(env, trimmed, " err") != NULL ||
                                p101_strstr(env, trimmed, "(err") != NULL || p101_strstr(env, trimmed, ", err") != NULL))
        {
            p101_module_map_note_error_use(env, err, map, file);
        }

        if(!file->is_header && (p101_strstr(env, trimmed, "P101_ERROR_") != NULL || p101_strstr(env, trimmed, "p101_error_has_") != NULL || p101_strstr(env, trimmed, "p101_error_is_") != NULL || p101_strstr(env, trimmed, "p101_error_reset") != NULL))
        {
            p101_module_map_note_error_check(env, err, map, file);
        }

        if(!file->is_header && p101_strchr(env, trimmed, '"') == NULL && p101_strstr(env, trimmed, "p101_error_create") != NULL)
        {
            p101_module_map_note_error_create(env, err, map, file);
        }

        if(!file->is_header && p101_strchr(env, trimmed, '"') == NULL && p101_strstr(env, trimmed, "p101_error_destroy") != NULL)
        {
            p101_module_map_note_error_destroy(env, err, map, file);
        }

        if(!file->is_header && p101_strchr(env, trimmed, '"') == NULL && p101_strstr(env, trimmed, "p101_env_create") != NULL)
        {
            p101_module_map_note_env_create(env, err, map, file);
        }

        if(!file->is_header && p101_strchr(env, trimmed, '"') == NULL && p101_strstr(env, trimmed, "p101_env_destroy") != NULL)
        {
            p101_module_map_note_env_destroy(env, err, map, file);
        }

        if(p101_module_map_parse_include_line(env, include_target, sizeof(include_target), &is_local, trimmed))
        {
            p101_module_map_add_include(env, err, map, file, include_target, line_number, is_local);
            continue;
        }

        if(file->is_header && p101_module_map_parse_macro_line(env, public_name, sizeof(public_name), trimmed))
        {
            p101_module_map_add_macro(env, err, map, file, public_name, line_number);
            continue;
        }

        if(trimmed[0] == '#' || trimmed[0] == '\0')
        {
            signature[0] = '\0';
            continue;
        }

        if(file->is_header && p101_module_map_parse_type_line(env, public_name, sizeof(public_name), trimmed))
        {
            p101_module_map_add_type(env, err, map, file, public_name, line_number);
        }

        p101_module_map_scan_call_tokens(env, err, map, file, trimmed, line_number);

        if(p101_strchr(env, trimmed, '(') != NULL || signature[0] != '\0')
        {
            p101_module_map_append_string(env, signature, sizeof(signature), " ");
            p101_module_map_append_string(env, signature, sizeof(signature), trimmed);
        }

        if(signature[0] != '\0' && p101_strchr(env, trimmed, ';') != NULL)
        {
            if(file->is_header && p101_module_map_parse_function_signature(env, function_name, sizeof(function_name), &is_static, signature))
            {
                p101_module_map_add_function(env, err, map, file, function_name, line_number, is_static, true);
            }
            signature[0] = '\0';
        }
        else if(signature[0] != '\0' && p101_strchr(env, trimmed, '{') != NULL)
        {
            if(p101_module_map_parse_function_signature(env, function_name, sizeof(function_name), &is_static, signature))
            {
                p101_module_map_add_function(env, err, map, file, function_name, line_number, is_static, false);
            }
            signature[0] = '\0';
        }
    }

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}
