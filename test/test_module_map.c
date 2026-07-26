#include "unity.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stdlib.h>

#define main p101_module_map_program_main
#include "../src/main.c"
#undef main

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

static void test_basename_no_suffix(void)
{
    char name[MAX_NAME];

    basename_no_suffix(env, name, sizeof(name), "src/server.c");
    TEST_ASSERT_EQUAL_STRING("server", name);

    include_to_module(env, name, sizeof(name), "p101/foo/bar.h");
    TEST_ASSERT_EQUAL_STRING("bar", name);
}

static void test_parse_include_line(void)
{
    char target[MAX_NAME];
    bool is_local;

    TEST_ASSERT_TRUE(parse_include_line(env, target, sizeof(target), &is_local, "#include \"server.h\""));
    TEST_ASSERT_TRUE(is_local);
    TEST_ASSERT_EQUAL_STRING("server.h", target);

    TEST_ASSERT_TRUE(parse_include_line(env, target, sizeof(target), &is_local, "#include <stdio.h>"));
    TEST_ASSERT_FALSE(is_local);
    TEST_ASSERT_EQUAL_STRING("stdio.h", target);
}

static void test_parse_function_signature(void)
{
    char name[MAX_NAME];
    bool is_static;

    TEST_ASSERT_TRUE(parse_function_signature(env, name, sizeof(name), &is_static, "static void helper(const char *name) {"));
    TEST_ASSERT_TRUE(is_static);
    TEST_ASSERT_EQUAL_STRING("helper", name);

    TEST_ASSERT_TRUE(parse_function_signature(env, name, sizeof(name), &is_static, "int server_run(struct config *config)"));
    TEST_ASSERT_FALSE(is_static);
    TEST_ASSERT_EQUAL_STRING("server_run", name);

    TEST_ASSERT_FALSE(parse_function_signature(env, name, sizeof(name), &is_static, "if(foo) {"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_basename_no_suffix);
    RUN_TEST(test_parse_include_line);
    RUN_TEST(test_parse_function_signature);
    return UNITY_END();
}
