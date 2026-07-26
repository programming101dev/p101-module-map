#include "parse.h"
#include "strings.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_string.h>
#include <stdbool.h>

static bool p101_module_map_is_identifier_start(const struct p101_env *env, char ch);
static bool p101_module_map_is_identifier_char(const struct p101_env *env, char ch);
static bool p101_module_map_line_starts_keyword(const struct p101_env *env, const char *line, const char *keyword, size_t keyword_len);
static bool p101_module_map_copy_identifier(const struct p101_env *env, char *name, size_t name_size, const char *start);

static bool p101_module_map_is_identifier_start(const struct p101_env *env, char ch)
{
    bool ret_val;

    ret_val = false;
    if(p101_isalpha(env, (unsigned char)ch) != 0 || ch == '_')
    {
        ret_val = true;
    }
    return ret_val;
}

static bool p101_module_map_is_identifier_char(const struct p101_env *env, char ch)
{
    bool ret_val;

    ret_val = false;
    if(p101_isalnum(env, (unsigned char)ch) != 0 || ch == '_')
    {
        ret_val = true;
    }
    return ret_val;
}

static bool p101_module_map_line_starts_keyword(const struct p101_env *env, const char *line, const char *keyword, size_t keyword_len)
{
    bool ret_val;

    ret_val = false;
    if(p101_strncmp(env, line, keyword, keyword_len) == 0 && (line[keyword_len] == '\0' || p101_isspace(env, (unsigned char)line[keyword_len])))
    {
        ret_val = true;
    }

    return ret_val;
}

static bool p101_module_map_copy_identifier(const struct p101_env *env, char *name, size_t name_size, const char *start)
{
    size_t len;
    bool   ret_val;

    len     = 0U;
    ret_val = false;
    if(!p101_module_map_is_identifier_start(env, start[0]))
    {
        goto done;
    }

    while(p101_module_map_is_identifier_char(env, start[len]))
    {
        len++;
    }

    if(len > 0U && len < name_size)
    {
        for(size_t i = 0; i < len; i++)
        {
            name[i] = start[i];
        }
        name[len] = '\0';
        ret_val   = true;
    }

done:
    return ret_val;
}

bool p101_module_map_is_source_suffix(const struct p101_env *env, const char *path)
{
    size_t len;
    bool   ret_val;

    len     = p101_strlen(env, path);
    ret_val = false;
    if(len > 2U && p101_strcmp(env, path + len - 2U, ".c") == 0)
    {
        ret_val = true;
    }
    return ret_val;
}

bool p101_module_map_is_header_suffix(const struct p101_env *env, const char *path)
{
    size_t len;
    bool   ret_val;

    len     = p101_strlen(env, path);
    ret_val = false;
    if(len > 2U && p101_strcmp(env, path + len - 2U, ".h") == 0)
    {
        ret_val = true;
    }
    return ret_val;
}

bool p101_module_map_is_skipped_dir(const struct p101_env *env, const char *name)
{
    bool ret_val;

    ret_val = false;
    if(p101_strcmp(env, name, ".git") == 0 || p101_strcmp(env, name, "build") == 0 || p101_strncmp(env, name, "build-", BUILD_PREFIX_LEN) == 0 || p101_strcmp(env, name, "cmake-build-debug") == 0 || p101_strcmp(env, name, "coverage") == 0 ||
       p101_strncmp(env, name, "coverage-", COVERAGE_PREFIX_LEN) == 0 || p101_strcmp(env, name, "profile") == 0 || p101_strncmp(env, name, "profile-", PROFILE_PREFIX_LEN) == 0)
    {
        ret_val = true;
    }
    return ret_val;
}

bool p101_module_map_parse_include_line(const struct p101_env *env, char *target, size_t target_size, bool *is_local, const char *line)
{
    const char *cursor;
    char        terminator;
    size_t      i;
    bool        ret_val;

    ret_val = false;
    if(p101_strncmp(env, line, "#include", INCLUDE_DIRECTIVE_LEN) != 0)
    {
        goto done;
    }

    cursor = line + INCLUDE_DIRECTIVE_LEN;
    while(*cursor != '\0' && p101_isspace(env, (unsigned char)*cursor))
    {
        cursor++;
    }

    if(*cursor == '"')
    {
        *is_local  = true;
        terminator = '"';
        cursor++;
    }
    else if(*cursor == '<')
    {
        *is_local  = false;
        terminator = '>';
        cursor++;
    }
    else
    {
        goto done;
    }

    i = 0U;
    while(*cursor != '\0' && *cursor != terminator && i + 1U < target_size)
    {
        target[i++] = *cursor++;
    }
    target[i] = '\0';
    ret_val   = false;
    if(i > 0U && *cursor == terminator)
    {
        ret_val = true;
    }

done:
    return ret_val;
}

bool p101_module_map_parse_macro_line(const struct p101_env *env, char *name, size_t name_size, const char *line)
{
    const char *cursor;
    bool        ret_val;

    ret_val = false;
    if(p101_strncmp(env, line, "#define", DEFINE_DIRECTIVE_LEN) != 0)
    {
        goto done;
    }

    cursor = line + DEFINE_DIRECTIVE_LEN;
    if(!p101_isspace(env, (unsigned char)*cursor))
    {
        goto done;
    }

    while(*cursor != '\0' && p101_isspace(env, (unsigned char)*cursor))
    {
        cursor++;
    }

    ret_val = p101_module_map_copy_identifier(env, name, name_size, cursor);
    if(ret_val)
    {
        size_t len;

        len = p101_strlen(env, name);
        if(len > 2U && name[len - 2U] == '_' && name[len - 1U] == 'H')
        {
            ret_val = false;
        }
    }

done:
    return ret_val;
}

bool p101_module_map_parse_type_line(const struct p101_env *env, char *name, size_t name_size, const char *line)
{
    const char *cursor;
    const char *end;
    const char *start;
    bool        ret_val;

    ret_val = false;
    cursor  = NULL;

    if(p101_module_map_line_starts_keyword(env, line, "typedef", TYPEDEF_KEYWORD_LEN))
    {
        end = p101_strchr(env, line, ';');
        if(end == NULL)
        {
            goto done;
        }

        while(end > line && !p101_module_map_is_identifier_char(env, end[-1]))
        {
            end--;
        }

        start = end;
        while(start > line && p101_module_map_is_identifier_char(env, start[-1]))
        {
            start--;
        }

        ret_val = p101_module_map_copy_identifier(env, name, name_size, start);
        goto done;
    }

    if(p101_module_map_line_starts_keyword(env, line, "struct", STRUCT_KEYWORD_LEN))
    {
        cursor = line + STRUCT_KEYWORD_LEN;
    }
    else if(p101_module_map_line_starts_keyword(env, line, "union", UNION_KEYWORD_LEN))
    {
        cursor = line + UNION_KEYWORD_LEN;
    }
    else if(p101_module_map_line_starts_keyword(env, line, "enum", ENUM_KEYWORD_LEN))
    {
        cursor = line + ENUM_KEYWORD_LEN;
    }

    if(cursor == NULL)
    {
        goto done;
    }

    while(*cursor != '\0' && p101_isspace(env, (unsigned char)*cursor))
    {
        cursor++;
    }

    if(*cursor == '{')
    {
        goto done;
    }

    ret_val = p101_module_map_copy_identifier(env, name, name_size, cursor);

done:
    return ret_val;
}

bool p101_module_map_parse_function_signature(const struct p101_env *env, char *name, size_t name_size, bool *is_static, const char *signature)
{
    const char *paren;
    const char *end;
    const char *start;
    const char *cursor;
    size_t      len;
    bool        ret_val;

    ret_val    = false;
    *is_static = false;
    paren      = p101_strchr(env, signature, '(');
    if(paren == NULL)
    {
        goto done;
    }

    end = paren;
    while(end > signature && p101_isspace(env, (unsigned char)end[-1]))
    {
        end--;
    }

    start = end;
    while(start > signature && p101_module_map_is_identifier_char(env, start[-1]))
    {
        start--;
    }

    len = (size_t)(end - start);
    if(len == 0U || len >= name_size)
    {
        goto done;
    }

    for(size_t i = 0; i < len; i++)
    {
        name[i] = start[i];
    }
    name[len] = '\0';

    if(p101_module_map_looks_like_control_keyword(env, name))
    {
        goto done;
    }

    cursor = signature;
    while(*cursor != '\0' && p101_isspace(env, (unsigned char)*cursor))
    {
        cursor++;
    }

    if(p101_strncmp(env, cursor, "static", STATIC_KEYWORD_LEN) == 0 && (cursor[STATIC_KEYWORD_LEN] == '\0' || p101_isspace(env, (unsigned char)cursor[STATIC_KEYWORD_LEN])))
    {
        *is_static = true;
    }

    ret_val = true;

done:
    return ret_val;
}

bool p101_module_map_looks_like_control_keyword(const struct p101_env *env, const char *name)
{
    bool ret_val;

    ret_val = false;
    if(p101_strcmp(env, name, "if") == 0 || p101_strcmp(env, name, "for") == 0 || p101_strcmp(env, name, "while") == 0 || p101_strcmp(env, name, "switch") == 0 || p101_strcmp(env, name, "return") == 0 || p101_strcmp(env, name, "sizeof") == 0)
    {
        ret_val = true;
    }
    return ret_val;
}
