#include "arguments.h"
#include "errors.h"
#include <dirent.h>
#include <limits.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_dirent.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

enum
{
    MAX_MODULES           = 512,
    MAX_FILES             = 1024,
    MAX_FUNCTIONS         = 4096,
    MAX_INCLUDES          = 4096,
    MAX_LINE              = 4096,
    MAX_SIGNATURE         = 8192,
    MAX_NAME              = 128,
    BUILD_PREFIX_LEN      = 6,
    COVERAGE_PREFIX_LEN   = 9,
    PROFILE_PREFIX_LEN    = 8,
    INCLUDE_DIRECTIVE_LEN = 8,
    DEFAULT_MAX_FUNCS     = 12,
    DEFAULT_MAX_PUB       = 8
};

struct source_file
{
    char path[PATH_LEN];
    char module[MAX_NAME];
    bool is_header;
};

struct module
{
    char   name[MAX_NAME];
    char   source_path[PATH_LEN];
    char   header_path[PATH_LEN];
    size_t source_count;
    size_t header_count;
    size_t function_count;
    size_t public_function_count;
    size_t static_function_count;
    size_t header_declaration_count;
    size_t local_include_count;
    size_t external_include_count;
};

struct function_record
{
    char   name[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    bool   is_static;
    bool   is_header_declaration;
};

struct include_record
{
    char   from_module[MAX_NAME];
    char   target[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    bool   is_local;
};

struct project_map
{
    struct source_file     files[MAX_FILES];
    struct module          modules[MAX_MODULES];
    struct function_record functions[MAX_FUNCTIONS];
    struct include_record  includes[MAX_INCLUDES];
    size_t                 file_count;
    size_t                 module_count;
    size_t                 function_count;
    size_t                 include_count;
};

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
static void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static int  run_module_map(const struct p101_env *env, struct p101_error *err, const struct arguments *args);

static void scan_path(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path);         // NOLINT(misc-no-recursion)
static void scan_directory(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path);    // NOLINT(misc-no-recursion)
static void scan_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path);
static void parse_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file);

static struct module *get_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *name);
static void           add_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, bool is_header);
static void           add_include(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *target, size_t line, bool is_local);
static void           add_function(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line, bool is_static, bool is_header_declaration);

static void write_report(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map);
static void write_modules(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map);
static void write_include_graph(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map);
static void write_findings(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map);
static void write_functions_for_module(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map, const char *module_name);

static bool is_source_suffix(const struct p101_env *env, const char *path);
static bool is_header_suffix(const struct p101_env *env, const char *path);
static bool is_skipped_dir(const struct p101_env *env, const char *name);
static bool parse_include_line(const struct p101_env *env, char *target, size_t target_size, bool *is_local, const char *line);
static bool parse_function_signature(const struct p101_env *env, char *name, size_t name_size, bool *is_static, const char *signature);
static bool looks_like_control_keyword(const struct p101_env *env, const char *name);
static bool module_has_direct_include(const struct p101_env *env, const struct project_map *map, const char *from, const char *target);
static bool function_used_outside_module(const struct p101_env *env, const struct project_map *map, const struct function_record *function);

static void           copy_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source);
static void           append_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source);
static void           basename_no_suffix(const struct p101_env *env, char *destination, size_t destination_size, const char *path);
static void           include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const char *include_name);
static char          *trim_left(const struct p101_env *env, char *text);
static void           trim_right(const struct p101_env *env, char *text);
static const char    *path_basename(const struct p101_env *env, const char *path);
_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

int main(int argc, char *argv[])
{
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;
    int                ret_val;

    ret_val = EXIT_FAILURE;
    err     = p101_error_create(false);
    env     = p101_env_create(err, NULL);
    p101_memset(env, &args, 0, sizeof(args));
    args.max_functions = DEFAULT_MAX_FUNCS;
    args.max_public    = DEFAULT_MAX_PUB;

    parse_arguments(env, err, argc, argv, &args);

    if(p101_error_has_no_error(err))
    {
        check_arguments(env, err, &args);
    }

    if(p101_error_has_no_error(err))
    {
        ret_val = run_module_map(env, err, &args);
    }

    if(p101_error_has_error(err))
    {
        if(p101_error_is_error(err, P101_ERROR_USER, ERR_USAGE))
        {
            usage(env, err, argv[0], EXIT_FAILURE, p101_error_get_message(err));
        }

        p101_fprintf(env, err, stderr, "%s\n", p101_error_get_message(err));
        ret_val = EXIT_FAILURE;
    }

    p101_env_destroy(env);
    p101_error_destroy(err);
    return ret_val;
}

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);
    opterr = 0;

    while((opt = p101_getopt(env, argc, argv, ":hvo:m:p:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                usage(env, err, argv[0], EXIT_SUCCESS, NULL);
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

static void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE(env);

    if(args->output_path != NULL && args->output_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The output path must not be empty.", ERR_USAGE);
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

static int run_module_map(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    static struct project_map map;
    FILE                     *stream;
    int                       ret_val;

    P101_TRACE(env);
    p101_memset(env, &map, 0, sizeof(map));
    stream  = stdout;
    ret_val = EXIT_FAILURE;

    if(args->path_count == 0U)
    {
        scan_path(env, err, &map, ".");
    }
    else
    {
        for(size_t i = 0; i < args->path_count && p101_error_has_no_error(err); i++)
        {
            scan_path(env, err, &map, args->paths[i]);
        }
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    for(size_t i = 0; i < map.file_count && p101_error_has_no_error(err); i++)
    {
        parse_source_file(env, err, &map, &map.files[i]);
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(args->output_path != NULL)
    {
        stream = p101_fopen(env, err, args->output_path, "w");
        if(stream == NULL)
        {
            goto done;
        }
    }

    write_report(env, err, stream, args, &map);
    if(p101_error_has_no_error(err))
    {
        ret_val = EXIT_SUCCESS;
    }
    else
    {
        ret_val = EXIT_FAILURE;
    }

done:
    if(stream != NULL && stream != stdout)
    {
        p101_fclose(env, err, stream);
    }
    return ret_val;
}

static void scan_path(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path)    // NOLINT(misc-no-recursion)
{
    struct stat info;

    P101_TRACE(env);
    if(p101_stat(env, err, path, &info) != 0)
    {
        goto done;
    }

    if(S_ISDIR(info.st_mode))
    {
        scan_directory(env, err, map, path);
    }
    else if(S_ISREG(info.st_mode))
    {
        scan_file(env, err, map, path);
    }

done:
    return;
}

static void scan_directory(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path)    // NOLINT(misc-no-recursion)
{
    DIR           *dir;
    struct dirent *entry;

    P101_TRACE(env);
    dir = p101_opendir(env, err, path);
    if(dir == NULL)
    {
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
            break;
        }

        if(p101_strcmp(env, entry->d_name, ".") == 0 || p101_strcmp(env, entry->d_name, "..") == 0 || is_skipped_dir(env, entry->d_name))
        {
            continue;
        }

        p101_snprintf(env, err, child, sizeof(child), "%s/%s", path, entry->d_name);
        scan_path(env, err, map, child);
    }

done:
    if(dir != NULL)
    {
        p101_closedir(env, err, dir);
    }
}

static void scan_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path)
{
    P101_TRACE(env);
    if(is_source_suffix(env, path))
    {
        add_source_file(env, err, map, path, false);
    }
    else if(is_header_suffix(env, path))
    {
        add_source_file(env, err, map, path, true);
    }
}

static void parse_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file)
{
    FILE  *stream;
    char   line[MAX_LINE];
    char   signature[MAX_SIGNATURE];
    size_t line_number;

    P101_TRACE(env);
    stream       = p101_fopen(env, err, file->path, "r");
    line_number  = 0U;
    signature[0] = '\0';

    if(stream == NULL)
    {
        goto done;
    }

    while(p101_fgets(env, err, line, sizeof(line), stream) != NULL && p101_error_has_no_error(err))
    {
        char  include_target[MAX_NAME];
        char  function_name[MAX_NAME];
        char *trimmed;
        bool  is_local;
        bool  is_static;

        line_number++;
        trimmed = trim_left(env, line);
        trim_right(env, trimmed);

        if(parse_include_line(env, include_target, sizeof(include_target), &is_local, trimmed))
        {
            add_include(env, err, map, file, include_target, line_number, is_local);
            continue;
        }

        if(trimmed[0] == '#' || trimmed[0] == '\0')
        {
            signature[0] = '\0';
            continue;
        }

        if(p101_strchr(env, trimmed, '(') != NULL || signature[0] != '\0')
        {
            append_string(env, signature, sizeof(signature), " ");
            append_string(env, signature, sizeof(signature), trimmed);
        }

        if(signature[0] != '\0' && p101_strchr(env, trimmed, ';') != NULL)
        {
            if(file->is_header && parse_function_signature(env, function_name, sizeof(function_name), &is_static, signature))
            {
                add_function(env, err, map, file, function_name, line_number, is_static, true);
            }
            signature[0] = '\0';
        }
        else if(signature[0] != '\0' && p101_strchr(env, trimmed, '{') != NULL)
        {
            if(parse_function_signature(env, function_name, sizeof(function_name), &is_static, signature))
            {
                add_function(env, err, map, file, function_name, line_number, is_static, false);
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

static struct module *get_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *name)
{
    struct module *module;

    P101_TRACE(env);
    module = NULL;

    for(size_t i = 0; i < map->module_count; i++)
    {
        if(p101_strcmp(env, map->modules[i].name, name) == 0)
        {
            module = &map->modules[i];
            goto done;
        }
    }

    if(map->module_count >= MAX_MODULES)
    {
        P101_ERROR_RAISE_USER(err, "Too many modules for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = &map->modules[map->module_count++];
    p101_memset(env, module, 0, sizeof(*module));
    copy_string(env, module->name, sizeof(module->name), name);

done:
    return module;
}

static void add_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, bool is_header)
{
    struct source_file *file;
    struct module      *module;
    char                module_name[MAX_NAME];

    P101_TRACE(env);
    if(map->file_count >= MAX_FILES)
    {
        P101_ERROR_RAISE_USER(err, "Too many files for p101-module-map.", ERR_USAGE);
        goto done;
    }

    basename_no_suffix(env, module_name, sizeof(module_name), path);
    module = get_module(env, err, map, module_name);
    if(module == NULL)
    {
        goto done;
    }

    file = &map->files[map->file_count++];
    copy_string(env, file->path, sizeof(file->path), path);
    copy_string(env, file->module, sizeof(file->module), module_name);
    file->is_header = is_header;

    if(is_header)
    {
        module->header_count++;
        if(module->header_path[0] == '\0')
        {
            copy_string(env, module->header_path, sizeof(module->header_path), path);
        }
    }
    else
    {
        module->source_count++;
        if(module->source_path[0] == '\0')
        {
            copy_string(env, module->source_path, sizeof(module->source_path), path);
        }
    }

done:
    return;
}

static void add_include(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *target, size_t line, bool is_local)
{
    struct include_record *include;
    struct module         *module;

    P101_TRACE(env);
    if(map->include_count >= MAX_INCLUDES)
    {
        P101_ERROR_RAISE_USER(err, "Too many includes for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    include = &map->includes[map->include_count++];
    copy_string(env, include->from_module, sizeof(include->from_module), file->module);
    copy_string(env, include->path, sizeof(include->path), file->path);
    include->line     = line;
    include->is_local = is_local;

    if(is_local)
    {
        include_to_module(env, include->target, sizeof(include->target), target);
        module->local_include_count++;
    }
    else
    {
        copy_string(env, include->target, sizeof(include->target), target);
        module->external_include_count++;
    }

done:
    return;
}

static void add_function(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line, bool is_static, bool is_header_declaration)
{
    struct function_record *function;
    struct module          *module;

    P101_TRACE(env);
    if(map->function_count >= MAX_FUNCTIONS)
    {
        P101_ERROR_RAISE_USER(err, "Too many functions for p101-module-map.", ERR_USAGE);
        goto done;
    }

    module = get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    function = &map->functions[map->function_count++];
    copy_string(env, function->name, sizeof(function->name), name);
    copy_string(env, function->module, sizeof(function->module), file->module);
    copy_string(env, function->path, sizeof(function->path), file->path);
    function->line                  = line;
    function->is_static             = is_static;
    function->is_header_declaration = is_header_declaration;

    if(is_header_declaration)
    {
        module->header_declaration_count++;
    }
    else
    {
        module->function_count++;
        if(is_static)
        {
            module->static_function_count++;
        }
        else
        {
            module->public_function_count++;
        }
    }

done:
    return;
}

static void write_report(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map)
{
    P101_TRACE(env);
    p101_fputs(env, err, "# p101 module map\n\n", stream);
    p101_fprintf(env, err, stream, "Files scanned: `%zu`\n\n", map->file_count);
    p101_fprintf(env, err, stream, "Modules found: `%zu`\n\n", map->module_count);
    p101_fprintf(env, err, stream, "Functions found: `%zu`\n\n", map->function_count);
    write_modules(env, err, stream, map);
    write_include_graph(env, err, stream, map);
    write_findings(env, err, stream, args, map);
}

static void write_modules(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map)
{
    P101_TRACE(env);
    p101_fputs(env, err, "## Modules\n\n", stream);
    p101_fputs(env, err, "| Module | Source | Header | Functions | Public | Static | Header declarations | Includes |\n", stream);
    p101_fputs(env, err, "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |\n", stream);

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;

        module = &map->modules[i];
        p101_fprintf(env,
                     err,
                     stream,
                     "| `%s` | %zu | %zu | %zu | %zu | %zu | %zu | %zu local / %zu external |\n",
                     module->name,
                     module->source_count,
                     module->header_count,
                     module->function_count,
                     module->public_function_count,
                     module->static_function_count,
                     module->header_declaration_count,
                     module->local_include_count,
                     module->external_include_count);
    }

    p101_fputs(env, err, "\n## Functions by module\n\n", stream);
    for(size_t i = 0; i < map->module_count; i++)
    {
        p101_fprintf(env, err, stream, "### `%s`\n\n", map->modules[i].name);
        write_functions_for_module(env, err, stream, map, map->modules[i].name);
    }
}

static void write_include_graph(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map)
{
    P101_TRACE(env);
    p101_fputs(env, err, "## Local include graph\n\n", stream);

    for(size_t i = 0; i < map->include_count; i++)
    {
        const struct include_record *include;

        include = &map->includes[i];
        if(include->is_local)
        {
            p101_fprintf(env, err, stream, "- `%s` -> `%s` (%s:%zu)\n", include->from_module, include->target, include->path, include->line);
        }
    }

    p101_fputs(env, err, "\n", stream);
}

static void write_findings(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map)
{
    bool wrote;

    P101_TRACE(env);
    wrote = false;
    p101_fputs(env, err, "## Teaching notes\n\n", stream);

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;

        module = &map->modules[i];
        if(module->source_count > 0U && module->header_count == 0U && p101_strcmp(env, module->name, "main") != 0)
        {
            p101_fprintf(env, err, stream, "- `%s` has source code but no matching header. If other modules should use it, create a small public interface.\n", module->name);
            wrote = true;
        }

        if(module->function_count > args->max_functions)
        {
            p101_fprintf(env, err, stream, "- `%s` has %zu functions. Consider splitting one responsibility into another module.\n", module->name, module->function_count);
            wrote = true;
        }

        if(module->public_function_count > args->max_public)
        {
            p101_fprintf(env, err, stream, "- `%s` exposes %zu non-static functions. Consider making helpers `static` or narrowing the public API.\n", module->name, module->public_function_count);
            wrote = true;
        }

        if(p101_strcmp(env, module->name, "main") == 0 && module->function_count > 3U)
        {
            p101_fprintf(env, err, stream, "- `main` has %zu functions. Students usually do better when `main.c` only wires argument parsing, setup, and top-level control flow.\n", module->function_count);
            wrote = true;
        }

        if((p101_strcmp(env, module->name, "util") == 0 || p101_strcmp(env, module->name, "utils") == 0) && module->function_count > 4U)
        {
            p101_fprintf(env, err, stream, "- `%s` looks like a utility dumping ground. Try naming modules after responsibilities instead.\n", module->name);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;

        function = &map->functions[i];
        if(!function->is_header_declaration && !function->is_static && !function_used_outside_module(env, map, function) && p101_strcmp(env, function->name, "main") != 0)
        {
            p101_fprintf(env, err, stream, "- `%s` in `%s` is non-static and appears local to `%s`. If it is a helper, make it `static`.\n", function->name, function->path, function->module);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->include_count; i++)
    {
        const struct include_record *include;

        include = &map->includes[i];
        if(include->is_local && module_has_direct_include(env, map, include->target, include->from_module) && p101_strcmp(env, include->from_module, include->target) != 0)
        {
            p101_fprintf(env, err, stream, "- `%s` and `%s` include each other. That direct cycle is a design smell; introduce a smaller shared interface.\n", include->from_module, include->target);
            wrote = true;
        }
    }

    if(!wrote)
    {
        p101_fputs(env, err, "No obvious module-structure issues found by the v1 heuristics.\n", stream);
    }
}

static void write_functions_for_module(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map, const char *module_name)
{
    bool wrote;

    P101_TRACE(env);
    wrote = false;

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;

        function = &map->functions[i];
        if(p101_strcmp(env, function->module, module_name) == 0)
        {
            const char *visibility;

            if(function->is_header_declaration)
            {
                visibility = "header";
            }
            else if(function->is_static)
            {
                visibility = "private";
            }
            else
            {
                visibility = "public";
            }

            p101_fprintf(env, err, stream, "- `%s` — %s (%s:%zu)\n", function->name, visibility, function->path, function->line);
            wrote = true;
        }
    }

    if(!wrote)
    {
        p101_fputs(env, err, "- No functions detected.\n", stream);
    }

    p101_fputs(env, err, "\n", stream);
}

static bool is_source_suffix(const struct p101_env *env, const char *path)
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

static bool is_header_suffix(const struct p101_env *env, const char *path)
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

static bool is_skipped_dir(const struct p101_env *env, const char *name)
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

static bool parse_include_line(const struct p101_env *env, char *target, size_t target_size, bool *is_local, const char *line)
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

static bool parse_function_signature(const struct p101_env *env, char *name, size_t name_size, bool *is_static, const char *signature)
{
    const char *paren;
    const char *end;
    const char *start;
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
    while(start > signature && (p101_isalnum(env, (unsigned char)start[-1]) || start[-1] == '_'))
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

    if(looks_like_control_keyword(env, name))
    {
        goto done;
    }

    *is_static = p101_strstr(env, signature, "static") != NULL;
    ret_val    = true;

done:
    return ret_val;
}

static bool looks_like_control_keyword(const struct p101_env *env, const char *name)
{
    bool ret_val;

    ret_val = false;
    if(p101_strcmp(env, name, "if") == 0 || p101_strcmp(env, name, "for") == 0 || p101_strcmp(env, name, "while") == 0 || p101_strcmp(env, name, "switch") == 0 || p101_strcmp(env, name, "return") == 0 || p101_strcmp(env, name, "sizeof") == 0)
    {
        ret_val = true;
    }
    return ret_val;
}

static bool module_has_direct_include(const struct p101_env *env, const struct project_map *map, const char *from, const char *target)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->include_count; i++)
    {
        if(map->includes[i].is_local && p101_strcmp(env, map->includes[i].from_module, from) == 0 && p101_strcmp(env, map->includes[i].target, target) == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

static bool function_used_outside_module(const struct p101_env *env, const struct project_map *map, const struct function_record *function)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->function_count; i++)
    {
        if(map->functions[i].is_header_declaration && p101_strcmp(env, map->functions[i].name, function->name) == 0 && p101_strcmp(env, map->functions[i].module, function->module) == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

static void copy_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source)
{
    P101_TRACE(env);
    if(destination_size > 0U)
    {
        p101_strncpy(env, destination, source, destination_size - 1U);
        destination[destination_size - 1U] = '\0';
    }
}

static void append_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source)
{
    size_t used;

    P101_TRACE(env);
    used = p101_strlen(env, destination);
    if(used + 1U < destination_size)
    {
        copy_string(env, destination + used, destination_size - used, source);
    }
}

static void basename_no_suffix(const struct p101_env *env, char *destination, size_t destination_size, const char *path)
{
    const char *base;
    const char *dot;
    size_t      len;

    P101_TRACE(env);
    base = path_basename(env, path);
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

static void include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const char *include_name)
{
    basename_no_suffix(env, destination, destination_size, include_name);
}

static char *trim_left(const struct p101_env *env, char *text)
{
    while(*text != '\0' && p101_isspace(env, (unsigned char)*text))
    {
        text++;
    }
    return text;
}

static void trim_right(const struct p101_env *env, char *text)
{
    size_t len;

    len = p101_strlen(env, text);
    while(len > 0U && p101_isspace(env, (unsigned char)text[len - 1U]))
    {
        text[--len] = '\0';
    }
}

static const char *path_basename(const struct p101_env *env, const char *path)
{
    const char *slash;

    slash = p101_strrchr(env, path, '/');
    return (slash == NULL) ? path : slash + 1;
}

static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-o <report.md>] [-m <max-functions>] [-p <max-public>] [path...]\n", program_name);
    p101_fputs(env, err, "\nMap source/header modules and point out beginner-friendly module design issues.\n", stderr);
    p101_fputs(env, err, "\nOptions:\n", stderr);
    p101_fputs(env, err, "  -h                 Show this help\n", stderr);
    p101_fputs(env, err, "  -v                 Enable p101 tracing inside p101-module-map\n", stderr);
    p101_fputs(env, err, "  -o <report.md>     Write Markdown report to a file instead of stdout\n", stderr);
    p101_fputs(env, err, "  -m <count>         Warn when a module has more than this many functions; default 12\n", stderr);
    p101_fputs(env, err, "  -p <count>         Warn when a module exposes more than this many non-static functions; default 8\n", stderr);
    p101_fputs(env, err, "\nExample:\n", stderr);
    p101_fprintf(env, err, stderr, "  %s -o module-map.md src include\n", program_name);
    p101_exit(env, exit_code);
}
