#include "cli.h"
#include "constants.h"
#include "model.h"
#include "model_mutation.h"
#include "model_query.h"
#include "strings.h"
#include "unity.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_posix/p101_unistd.h>
#include <stdbool.h>
#include <stdlib.h>

static struct p101_error *error;
static struct p101_env   *env;

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
}

void tearDown(void)
{
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void reset_getopt(void)
{
#ifdef __GLIBC__
    optind = 0;
#else
    extern int optreset;
    optreset = 1;
    optind   = 1;
#endif
}

static void test_parse_accepts_layer_config(void)
{
    char            *argv[] = {"p101-module-map", "-l", "layers.txt", "src", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 4, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("layers.txt", args.layer_config_path);
    TEST_ASSERT_EQUAL_STRING("src", args.paths[0]);
}

static void test_parse_accepts_compile_database(void)
{
    char            *argv[] = {"p101-module-map", "-C", "build-clang/compile_commands.json", "src", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 4, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("build-clang/compile_commands.json", args.compile_db_path);
    TEST_ASSERT_EQUAL_STRING("src", args.paths[0]);
}

static void test_parse_accepts_library_mode(void)
{
    char            *argv[] = {"p101-module-map", "-L", "src", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 3, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_TRUE(args.library_mode);
    TEST_ASSERT_EQUAL_STRING("src", args.paths[0]);
}

static void test_parse_accepts_json_fact_snapshot(void)
{
    char            *argv[] = {"p101-module-map", "-j", "-i", "facts.tsv", "src", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 5, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_TRUE(args.json);
    TEST_ASSERT_EQUAL_STRING("facts.tsv", args.facts_path);
}

static void test_fact_snapshot_conflicts_with_compile_database(void)
{
    char            *argv[] = {"p101-module-map", "-i", "facts.tsv", "-C", "compile_commands.json", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 5, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_TRUE(p101_error_has_error(error));
}

static void test_basename_no_suffix(void)
{
    char name[MAX_NAME];

    p101_module_map_basename_no_suffix(env, name, sizeof(name), "src/server.c");
    TEST_ASSERT_EQUAL_STRING("server", name);

    p101_module_map_include_to_module(env, name, sizeof(name), "p101/foo/bar.h");
    TEST_ASSERT_EQUAL_STRING("foo/bar", name);

    p101_module_map_include_to_module(env, name, sizeof(name), "p101/foo/p101_widget.h");
    TEST_ASSERT_EQUAL_STRING("foo/widget", name);

    p101_module_map_normalize_module_name(env, name, sizeof(name), "lib_posix/p101_mman");
    TEST_ASSERT_EQUAL_STRING("lib_posix/mman", name);
}

static void test_public_function_requires_used_interface(void)
{
    static struct project_map map;
    struct source_file        source;
    struct source_file        header;
    struct source_file        caller;
    struct function_record   *function;

    p101_memset(env, &map, 0, sizeof(map));
    p101_memset(env, &source, 0, sizeof(source));
    p101_memset(env, &header, 0, sizeof(header));
    p101_memset(env, &caller, 0, sizeof(caller));

    p101_module_map_copy_string(env, source.path, sizeof(source.path), "src/tool.c");
    p101_module_map_copy_string(env, source.module, sizeof(source.module), "tool");
    source.is_header = false;

    p101_module_map_copy_string(env, header.path, sizeof(header.path), "include/tool.h");
    p101_module_map_copy_string(env, header.module, sizeof(header.module), "tool");
    header.is_header = true;

    p101_module_map_copy_string(env, caller.path, sizeof(caller.path), "src/main.c");
    p101_module_map_copy_string(env, caller.module, sizeof(caller.module), "main");
    caller.is_header = false;

    p101_module_map_add_source_file(env, error, &map, source.path, source.is_header);
    p101_module_map_add_source_file(env, error, &map, header.path, header.is_header);
    p101_module_map_add_source_file(env, error, &map, caller.path, caller.is_header);
    p101_module_map_add_function(env, error, &map, &source, "tool_run", 10, false, false);
    p101_module_map_add_function(env, error, &map, &header, "tool_run", 5, false, true);

    function = &map.functions[0];
    TEST_ASSERT_TRUE(p101_module_map_function_has_header_declaration(env, &map, function));
    TEST_ASSERT_FALSE(p101_module_map_function_used_outside_module(env, &map, function));

    p101_module_map_add_include(env, error, &map, &caller, "tool.h", 3, true);
    p101_module_map_add_call(env, error, &map, &caller, "tool_run", 4);
    TEST_ASSERT_TRUE(p101_module_map_function_used_outside_module(env, &map, function));
    TEST_ASSERT_TRUE(p101_module_map_function_has_non_static_definition(env, &map, &map.functions[1]));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_accepts_layer_config);
    RUN_TEST(test_parse_accepts_compile_database);
    RUN_TEST(test_parse_accepts_library_mode);
    RUN_TEST(test_parse_accepts_json_fact_snapshot);
    RUN_TEST(test_fact_snapshot_conflicts_with_compile_database);
    RUN_TEST(test_basename_no_suffix);
    RUN_TEST(test_public_function_requires_used_interface);
    return UNITY_END();
}
