#include "constants.h"
#include "cli.h"
#include "model.h"
#include "model_mutation.h"
#include "model_query.h"
#include "parse.h"
#include "strings.h"
#include "unity.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stdlib.h>
#include <p101_posix/p101_unistd.h>

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

static void test_basename_no_suffix(void)
{
    char name[MAX_NAME];

    p101_module_map_basename_no_suffix(env, name, sizeof(name), "src/server.c");
    TEST_ASSERT_EQUAL_STRING("server", name);

    p101_module_map_include_to_module(env, name, sizeof(name), "p101/foo/bar.h");
    TEST_ASSERT_EQUAL_STRING("bar", name);
}

static void test_parse_include_line(void)
{
    char target[MAX_NAME];
    bool is_local;

    TEST_ASSERT_TRUE(p101_module_map_parse_include_line(env, target, sizeof(target), &is_local, "#include \"server.h\""));
    TEST_ASSERT_TRUE(is_local);
    TEST_ASSERT_EQUAL_STRING("server.h", target);

    TEST_ASSERT_TRUE(p101_module_map_parse_include_line(env, target, sizeof(target), &is_local, "#include <stdio.h>"));
    TEST_ASSERT_FALSE(is_local);
    TEST_ASSERT_EQUAL_STRING("stdio.h", target);
}

static void test_parse_public_macro_and_type_lines(void)
{
    char name[MAX_NAME];

    TEST_ASSERT_TRUE(p101_module_map_parse_macro_line(env, name, sizeof(name), "#define RESOURCE_LIMIT 32"));
    TEST_ASSERT_EQUAL_STRING("RESOURCE_LIMIT", name);

    TEST_ASSERT_TRUE(p101_module_map_parse_macro_line(env, name, sizeof(name), "#define RESOURCE_LIMIT(value) ((value) + 1)"));
    TEST_ASSERT_EQUAL_STRING("RESOURCE_LIMIT", name);

    TEST_ASSERT_FALSE(p101_module_map_parse_macro_line(env, name, sizeof(name), "#define P101_MODULE_MAP_MODEL_H"));

    TEST_ASSERT_TRUE(p101_module_map_parse_type_line(env, name, sizeof(name), "typedef struct config config;"));
    TEST_ASSERT_EQUAL_STRING("config", name);

    TEST_ASSERT_TRUE(p101_module_map_parse_type_line(env, name, sizeof(name), "struct server_state"));
    TEST_ASSERT_EQUAL_STRING("server_state", name);
}

static void test_parse_function_signature(void)
{
    char name[MAX_NAME];
    bool is_static;

    TEST_ASSERT_TRUE(p101_module_map_parse_function_signature(env, name, sizeof(name), &is_static, "static void helper(const char *name) {"));
    TEST_ASSERT_TRUE(is_static);
    TEST_ASSERT_EQUAL_STRING("helper", name);

    TEST_ASSERT_TRUE(p101_module_map_parse_function_signature(env, name, sizeof(name), &is_static, "int server_run(struct config *config)"));
    TEST_ASSERT_FALSE(is_static);
    TEST_ASSERT_EQUAL_STRING("server_run", name);

    TEST_ASSERT_TRUE(p101_module_map_parse_function_signature(env, name, sizeof(name), &is_static, "void add_function(bool is_static)"));
    TEST_ASSERT_FALSE(is_static);
    TEST_ASSERT_EQUAL_STRING("add_function", name);

    TEST_ASSERT_FALSE(p101_module_map_parse_function_signature(env, name, sizeof(name), &is_static, "if(foo) {"));
}

static void test_public_function_requires_used_interface(void)
{
    struct project_map      map;
    struct source_file      source;
    struct source_file      header;
    struct source_file      caller;
    struct function_record *function;

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
    TEST_ASSERT_FALSE(p101_module_map_function_used_outside_module(env, &map, function));

    p101_module_map_add_include(env, error, &map, &caller, "tool.h", 3, true);
    TEST_ASSERT_TRUE(p101_module_map_function_used_outside_module(env, &map, function));
    TEST_ASSERT_TRUE(p101_module_map_function_has_non_static_definition(env, &map, &map.functions[1]));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_accepts_layer_config);
    RUN_TEST(test_basename_no_suffix);
    RUN_TEST(test_parse_include_line);
    RUN_TEST(test_parse_public_macro_and_type_lines);
    RUN_TEST(test_parse_function_signature);
    RUN_TEST(test_public_function_requires_used_interface);
    return UNITY_END();
}
