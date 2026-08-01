#include "fact_command.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/project.h>
#include <p101_filesystem/filesystem.h>
#include <stdbool.h>
#include <unistd.h>

static void        append_include_roots(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *command, const char *path);
static const char *choose_fact_tool(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static bool        executable_exists(const struct p101_env *env, struct p101_error *err, const char *path);

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
    return executable_exists(env, err, path);
}
#endif

static void append_include_roots(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *command, const char *path)
{
    char include_arg[PATH_LEN + sizeof("--cflag=-I/../include")];

    P101_TRACE_SCOPE(env);
    p101_snprintf(env, err, include_arg, sizeof(include_arg), "--cflag=-I%s", path);
    if(p101_error_has_no_error(err))
    {
        (void)p101_tool_argv_append(env, err, command, include_arg);
    }
    p101_snprintf(env, err, include_arg, sizeof(include_arg), "--cflag=-I%s/include", path);
    if(p101_error_has_no_error(err))
    {
        (void)p101_tool_argv_append(env, err, command, include_arg);
    }
    p101_snprintf(env, err, include_arg, sizeof(include_arg), "--cflag=-I%s/../include", path);
    if(p101_error_has_no_error(err))
    {
        (void)p101_tool_argv_append(env, err, command, include_arg);
    }
}

static const char *choose_fact_tool(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    const char *tool;

    P101_TRACE_SCOPE(env);
    tool = args->fact_tool_path;
    if(tool != NULL && tool[0] != '\0')
    {
        return tool;
    }

    tool = p101_getenv(env, "P101_MODULE_MAP_FACT_TOOL");
    if(tool != NULL && tool[0] != '\0')
    {
        return tool;
    }
    tool = p101_getenv(env, "P101_WRAPPER_AUDIT");
    if(tool != NULL && tool[0] != '\0')
    {
        return tool;
    }

    tool = "../p101-wrapper-audit/p101-wrapper-audit";
    if(executable_exists(env, err, tool))
    {
        return tool;
    }
    tool = "../../programs/p101-wrapper-audit/p101-wrapper-audit";
    if(executable_exists(env, err, tool))
    {
        return tool;
    }
    return "p101-wrapper-audit";
}

static bool executable_exists(const struct p101_env *env, struct p101_error *err, const char *path)
{
    bool ret_val;

    P101_TRACE_SCOPE(env);
#ifdef P101_MODULE_MAP_TESTING
    if(p101_module_map_test_executable_mode != 0)
    {
        if(p101_module_map_test_executable_mode == 4)
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
    if(p101_error_has_error(err))
    {
        p101_error_reset(err);
    }
#ifdef P101_MODULE_MAP_TESTING
done:
#endif
    return ret_val;
}

void p101_module_map_build_fact_argv(const struct p101_env *env, struct p101_error *err, struct p101_tool_argv *command, const struct arguments *args)
{
    char        discovered_compile_db[PATH_LEN];
    const char *compile_db;

    P101_TRACE_SCOPE(env);
    p101_tool_argv_init(command);
    if(p101_error_has_error(err))
    {
        return;
    }
    (void)p101_tool_argv_append(env, err, command, choose_fact_tool(env, err, args));
    (void)p101_tool_argv_append(env, err, command, "--emit-module-facts");
    compile_db = args->compile_db_path;
    if(compile_db == NULL &&
#ifdef P101_MODULE_MAP_TESTING
       ((p101_module_map_test_compile_db_mode == 1 && (p101_strncpy(env, discovered_compile_db, "compile_commands.json", sizeof(discovered_compile_db) - 1U), discovered_compile_db[sizeof(discovered_compile_db) - 1U] = '\0', true)) ||
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
        (void)p101_tool_argv_append(env, err, command, "--compile-db");
        (void)p101_tool_argv_append(env, err, command, compile_db);
        (void)p101_tool_argv_append(env, err, command, "--compile-db-only");
    }
    append_include_roots(env, err, command, ".");
    append_include_roots(env, err, command, "include");
    if(args->path_count == 0U)
    {
        append_include_roots(env, err, command, ".");
        (void)p101_tool_argv_append(env, err, command, ".");
    }
    else
    {
        for(size_t index = 0U; index < args->path_count && p101_error_has_no_error(err); index++)
        {
            append_include_roots(env, err, command, args->paths[index]);
        }
        for(size_t index = 0U; index < args->path_count && p101_error_has_no_error(err); index++)
        {
            (void)p101_tool_argv_append(env, err, command, args->paths[index]);
        }
    }
}
