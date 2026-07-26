set(PROJECT_NAME "p101-module-map")
set(PROJECT_VERSION "1.0.0")
set(PROJECT_DESCRIPTION "Programming 101 module map reporter")
set(PROJECT_LANGUAGE "C")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Common compiler flags
set(STANDARD_FLAGS
        -D_POSIX_C_SOURCE=200809L
        -D_XOPEN_SOURCE=700
        -Werror
)

set(DARWIN_STANDARD_FLAGS
        -D_DARWIN_C_SOURCE
)

set(LINUX_STANDARD_FLAGS
)

set(BSD_STANDARD_FLAGS
)

# Define targets
set(EXECUTABLE_TARGETS main)
set(LIBRARY_TARGETS "")
set(main_OUTPUT_NAME p101-module-map)

set(main_SOURCES
        src/cli.c
        src/fact_command.c
        src/fact_loader.c
        src/main.c
        src/model_mutation.c
        src/model_notes.c
        src/model_query.c
        src/report.c
        src/runner.c
        src/strings.c
)

set(main_HEADERS
        include/arguments.h
        include/cli.h
        include/constants.h
        include/errors.h
        include/fact_command.h
        include/fact_loader.h
        include/model.h
        include/model_mutation.h
        include/model_notes.h
        include/model_query.h
        include/report.h
        include/runner.h
        include/strings.h
)

set(main_LINK_LIBRARIES
        p101_error
        p101_env
        p101_c
        p101_c_facts
        p101_posix
        p101_unix
        p101_convert
        m
)
