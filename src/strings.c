#include "strings.h"
#include "constants.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_string.h>

void p101_module_map_copy_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source)
{
    P101_TRACE(env);
    if(destination_size > 0U)
    {
        p101_strncpy(env, destination, source, destination_size - 1U);
        destination[destination_size - 1U] = '\0';
    }
}

void p101_module_map_append_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source)
{
    size_t used;

    P101_TRACE(env);
    used = p101_strlen(env, destination);
    if(used + 1U < destination_size)
    {
        p101_module_map_copy_string(env, destination + used, destination_size - used, source);
    }
}

void p101_module_map_basename_no_suffix(const struct p101_env *env, char *destination, size_t destination_size, const char *path)
{
    const char *base;
    const char *dot;
    size_t      len;

    P101_TRACE(env);
    base = p101_module_map_path_basename(env, path);
    dot  = p101_strrchr(env, base, '.');
    len  = (dot == NULL) ? p101_strlen(env, base) : (size_t)(dot - base);
    if(len >= destination_size)
    {
        len = destination_size - 1U;
    }

    for(size_t i = 0; i < len; i++)
    {
        destination[i] = base[i];
    }
    destination[len] = '\0';
}

void p101_module_map_include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const char *include_name)
{
    p101_module_map_basename_no_suffix(env, destination, destination_size, include_name);
}

char *p101_module_map_trim_left(const struct p101_env *env, char *text)
{
    while(*text != '\0' && p101_isspace(env, (unsigned char)*text))
    {
        text++;
    }
    return text;
}

void p101_module_map_trim_right(const struct p101_env *env, char *text)
{
    size_t len;

    len = p101_strlen(env, text);
    while(len > 0U && p101_isspace(env, (unsigned char)text[len - 1U]))
    {
        text[--len] = '\0';
    }
}

const char *p101_module_map_path_basename(const struct p101_env *env, const char *path)
{
    const char *slash;

    slash = p101_strrchr(env, path, '/');
    return (slash == NULL) ? path : slash + 1;
}
