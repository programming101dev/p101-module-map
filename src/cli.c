#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_cli/cli.h>
#include <p101_convert/integer.h>
#include <stdlib.h>

void p101_module_map_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->max_functions = DEFAULT_MAX_FUNCS;
    args->max_public    = DEFAULT_MAX_PUB;
}

void p101_module_map_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;
#ifdef P101_MODULE_MAP_TESTING
    const char *forced_option;
#endif

    P101_TRACE_SCOPE(env);
    opterr = 0;
#ifdef P101_MODULE_MAP_TESTING
    forced_option = getenv("P101_MODULE_MAP_TEST_OPTION");
#endif

    if(argc == 2 && p101_strcmp(env, argv[1], "--help") == 0)
    {
        args->show_help = true;
        goto done;
    }

    while(
#ifdef P101_MODULE_MAP_TESTING
        (opt = forced_option == NULL ? p101_getopt(env, argc, argv, ":hjLvi:o:l:m:p:C:") : (unsigned char)*forced_option) != -1 &&
#else
        (opt = p101_getopt(env, argc, argv, ":hjLvi:o:l:m:p:C:")) != -1 &&
#endif
        p101_error_has_no_error(err))
    {
#ifdef P101_MODULE_MAP_TESTING
        forced_option = NULL;
#endif
        switch(opt)
        {
            case 'h':
            {
                args->show_help = true;
                break;
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'j':
            {
                args->json = true;
                break;
            }
            case 'i':
            {
                args->facts_path = optarg;
                break;
            }
            case 'L':
            {
                args->library_mode = true;
                break;
            }
            case 'o':
            {
                args->output_path = optarg;
                break;
            }
            case 'l':
            {
                args->layer_config_path = optarg;
                break;
            }
            case 'm':
            {
                args->max_functions = p101_parse_unsigned_long(env, err, optarg, DEFAULT_MAX_FUNCS);
                break;
            }
            case 'p':
            {
                args->max_public = p101_parse_unsigned_long(env, err, optarg, DEFAULT_MAX_PUB);
                break;
            }
            case 'C':
            {
                args->compile_db_path = optarg;
                break;
            }
            case ':':
            {
                char msg[MAX_NAME];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            default:    // GCOVR_EXCL_BR_LINE -- both labels intentionally share one diagnostic
            {
                char msg[MAX_NAME];

                p101_snprintf(env, err, msg, sizeof(msg), "Unknown option '-%c'.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    while(optind < argc && args->path_count < P101_MODULE_MAP_MAX_PATHS)
    {
        args->paths[args->path_count++] = argv[optind++];
    }

    if(optind < argc)
    {
        P101_ERROR_RAISE_USER(err, "Too many paths.", ERR_USAGE);
    }

done:
    return;
}

void p101_module_map_check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE_SCOPE(env);

    if(args->output_path != NULL && args->output_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The output path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->layer_config_path != NULL && args->layer_config_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The layer config path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->compile_db_path != NULL && args->compile_db_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The compile database path must not be empty.", ERR_USAGE);
        goto done;
    }
    if(args->facts_path != NULL && args->facts_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The facts path must not be empty.", ERR_USAGE);
        goto done;
    }
    if(args->facts_path != NULL && args->compile_db_path != NULL)
    {
        P101_ERROR_RAISE_USER(err, "The facts snapshot cannot be combined with -C.", ERR_USAGE);
        goto done;
    }

    if(args->max_functions == 0U)
    {
        P101_ERROR_RAISE_USER(err, "The max-functions threshold must be greater than zero.", ERR_USAGE);
        goto done;
    }

    if(args->max_public == 0U)
    {
        P101_ERROR_RAISE_USER(err, "The max-public threshold must be greater than zero.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

void p101_module_map_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    (void)exit_code;

    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-j] [-L] [-v] [-i <facts.tsv>] [-o <report>] [-l <layers.txt>] [-m <max-functions>] [-p <max-public>] [-C <compile_commands.json>] [path...]\n", program_name);
    p101_fputs(env, err, "\nMap source/header modules and point out beginner-friendly module design issues.\n", stderr);
    p101_fputs(env, err, "\nOptions:\n", stderr);
    p101_fputs(env, err, "  -h                 Show this help\n", stderr);
    p101_fputs(env, err, "  -j                 Emit normalized JSON findings instead of Markdown\n", stderr);
    p101_fputs(env, err, "  -L                 Library mode: do not infer public API use from this repo alone\n", stderr);
    p101_fputs(env, err, "  -v                 Enable p101 tracing inside p101-module-map\n", stderr);
    p101_fputs(env, err, "  -i <facts.tsv>     Read a reusable P101FACT v6 snapshot instead of invoking Clang\n", stderr);
    p101_fputs(env, err, "  -o <report>        Write the report to a file instead of stdout\n", stderr);
    p101_fputs(env, err, "  -l <layers.txt>    Optional allowed include edges, one `module -> module` per line\n", stderr);
    p101_fputs(env, err, "  -m <count>         Warn when a module has more than this many functions; default 12\n", stderr);
    p101_fputs(env, err, "  -p <count>         Warn when a module exposes more than this many non-static functions; default 8\n", stderr);
    p101_fputs(env, err, "  -C <file>          Compile database used by native lib_c_facts analysis\n", stderr);
    p101_fputs(env, err, "\nExample:\n", stderr);
    p101_fprintf(env, err, stderr, "  %s -o module-map.md src include\n", program_name);
}
