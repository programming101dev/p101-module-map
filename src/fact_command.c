#include "fact_command.h"
#include "constants.h"
#include "errors.h"
#include "strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/project.h>
#include <p101_filesystem/filesystem.h>
#include <stdbool.h>
#include <unistd.h>

static void        p101_module_map_append_checked(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *text);
static void        p101_module_map_append_char_checked(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, char ch);
static void        p101_module_map_append_shell_quoted(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *text);
static void        p101_module_map_append_cflag(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *flag);
static void        p101_module_map_append_compile_database(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *path);
static void        p101_module_map_append_include_roots(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *path);
static const char *p101_module_map_choose_fact_tool(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static bool        p101_module_map_executable_exists(const struct p101_env *env, struct p101_error *err, const char *path);
#ifdef P101_MODULE_MAP_TESTING
static int p101_module_map_test_executable_mode;
static int p101_module_map_test_compile_db_mode = -1;

void p101_module_map_test_set_fact_command_modes(int executable_mode, int compile_db_mode)
{
    p101_module_map_test_executable_mode = executable_mode;
    p101_module_map_test_compile_db_mode = compile_db_mode;
}

bool p101_module_map_test_executable_exists(const struct p101_env *env, struct p101_error *err, const char *path)
{
    return p101_module_map_executable_exists(env, err, path);
}
#endif

static void p101_module_map_append_checked(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *text)
{
    size_t used;
    size_t extra;

    P101_TRACE_SCOPE(env);
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

    P101_TRACE_SCOPE(env);
    text[0] = ch;
    text[1] = '\0';
    p101_module_map_append_checked(env, err, command, command_size, text);
}

static void p101_module_map_append_shell_quoted(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *text)
{
    P101_TRACE_SCOPE(env);
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
    P101_TRACE_SCOPE(env);
    p101_module_map_append_checked(env, err, command, command_size, " --cflag=");
    p101_module_map_append_shell_quoted(env, err, command, command_size, flag);
}

static void p101_module_map_append_compile_database(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *path)
{
    P101_TRACE_SCOPE(env);
    p101_module_map_append_checked(env, err, command, command_size, " --compile-db=");
    p101_module_map_append_shell_quoted(env, err, command, command_size, path);
}

static void p101_module_map_append_include_roots(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const char *path)
{
    char root[PATH_LEN];
    char include_arg[PATH_LEN + 3U];

    P101_TRACE_SCOPE(env);
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

    P101_TRACE_SCOPE(env);
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

    P101_TRACE_SCOPE(env);
#ifdef P101_MODULE_MAP_TESTING
    if(p101_module_map_test_executable_mode != 0)
    {
        if(p101_module_map_test_executable_mode == 4)    // GCOVR_EXCL_BR_LINE -- deterministic wrapper-failure test hook
        {
            P101_ERROR_RAISE_USER(err, "forced executable check failure", ERR_USAGE);
            ret_val = false;
            goto checked;
        }
        ret_val = (p101_module_map_test_executable_mode == 1 && p101_strcmp(env, path, "../p101-wrapper-audit/p101-wrapper-audit") == 0) ||
                  (p101_module_map_test_executable_mode == 2 && p101_strcmp(env, path, "../../programs/p101-wrapper-audit/p101-wrapper-audit") == 0);
        goto done;
    }
#endif
    ret_val = p101_access(env, err, path, X_OK) == 0;
#ifdef P101_MODULE_MAP_TESTING
checked:
#endif
    if(p101_error_has_error(err))    // GCOVR_EXCL_BR_LINE -- only an OS access failure sets this
    {
        p101_error_reset(err);
    }
#ifdef P101_MODULE_MAP_TESTING
done:
#endif
    return ret_val;
}

void p101_module_map_build_fact_command(const struct p101_env *env, struct p101_error *err, char *command, size_t command_size, const struct arguments *args)
{
    char        discovered_compile_db[PATH_LEN];
    const char *compile_db;
    const char *tool;

    P101_TRACE_SCOPE(env);
    command[0] = '\0';
    tool       = p101_module_map_choose_fact_tool(env, err, args);
    p101_module_map_append_shell_quoted(env, err, command, command_size, tool);
    p101_module_map_append_checked(env, err, command, command_size, " --emit-module-facts");
    compile_db = args->compile_db_path;
    if(compile_db == NULL &&
#ifdef P101_MODULE_MAP_TESTING
       ((p101_module_map_test_compile_db_mode == 1 && (p101_module_map_copy_string(env, discovered_compile_db, sizeof(discovered_compile_db), "compile_commands.json"), true)) ||
        (p101_module_map_test_compile_db_mode < 0 && p101_c_facts_find_clang_compile_database(env, err, ".", discovered_compile_db, sizeof(discovered_compile_db))))
#else
       p101_c_facts_find_clang_compile_database(env, err, ".", discovered_compile_db, sizeof(discovered_compile_db))
#endif
    )
    {
        compile_db = discovered_compile_db;
    }
    if(compile_db != NULL && p101_error_has_no_error(err))
    {
        p101_module_map_append_compile_database(env, err, command, command_size, compile_db);
        p101_module_map_append_checked(env, err, command, command_size, " --compile-db-only");
    }
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
