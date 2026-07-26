#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_unistd.h>
#include <stdlib.h>

void p101_module_map_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->max_functions = DEFAULT_MAX_FUNCS;
    args->max_public    = DEFAULT_MAX_PUB;
}

void p101_module_map_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);
    opterr = 0;

    while((opt = p101_getopt(env, argc, argv, ":hvo:l:m:p:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                p101_module_map_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
            }
            case 'v':
            {
                args->verbose = true;
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
            case ':':
            {
                char msg[MAX_NAME];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            default:
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
    P101_TRACE(env);

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
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-o <report.md>] [-l <layers.txt>] [-m <max-functions>] [-p <max-public>] [path...]\n", program_name);
    p101_fputs(env, err, "\nMap source/header modules and point out beginner-friendly module design issues.\n", stderr);
    p101_fputs(env, err, "\nOptions:\n", stderr);
    p101_fputs(env, err, "  -h                 Show this help\n", stderr);
    p101_fputs(env, err, "  -v                 Enable p101 tracing inside p101-module-map\n", stderr);
    p101_fputs(env, err, "  -o <report.md>     Write Markdown report to a file instead of stdout\n", stderr);
    p101_fputs(env, err, "  -l <layers.txt>    Optional allowed include edges, one `module -> module` per line\n", stderr);
    p101_fputs(env, err, "  -m <count>         Warn when a module has more than this many functions; default 12\n", stderr);
    p101_fputs(env, err, "  -p <count>         Warn when a module exposes more than this many non-static functions; default 8\n", stderr);
    p101_fputs(env, err, "\nExample:\n", stderr);
    p101_fprintf(env, err, stderr, "  %s -o module-map.md src include\n", program_name);
    p101_exit(env, exit_code);
}
