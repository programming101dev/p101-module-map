#ifndef P101_MODULE_MAP_CONSTANTS_H
#define P101_MODULE_MAP_CONSTANTS_H

enum
{
    MAX_MODULES           = 512,
    MAX_FILES             = 1024,
    MAX_FUNCTIONS         = 4096,
    MAX_INCLUDES          = 4096,
    MAX_MACROS            = 2048,
    MAX_TYPES             = 2048,
    MAX_CALLS             = 16384,
    MAX_LINE              = 4096,
    MAX_COMMAND           = 32768,
    MAX_SIGNATURE         = 8192,
    MAX_NAME              = 128,
    MAX_FACT_FIELDS       = 16,
    FACT_PREFIX_LEN       = 9,
    FACT_BASE_FIELD_COUNT = 7,
    FACT_TAG_IDX          = 0,
    FACT_KIND_IDX         = 2,
    FACT_PATH_IDX         = 3,
    FACT_IS_HEADER_IDX    = 5,
    FACT_LINE_IDX         = 6,
    FACT_VALUE_IDX        = 7,
    FACT_FLAG1_IDX        = 8,
    FACT_FLAG2_IDX        = 9,
    FACT_VALUE_FIELDS     = 8,
    FACT_INCLUDE_FIELDS   = 9,
    FACT_FUNCTION_FIELDS  = 10,
    P101_ERROR_HAS_LEN    = 15,
    P101_ERROR_IS_LEN     = 14,
    BUILD_PREFIX_LEN      = 6,
    COVERAGE_PREFIX_LEN   = 9,
    PROFILE_PREFIX_LEN    = 8,
    INCLUDE_DIRECTIVE_LEN = 8,
    DEFINE_DIRECTIVE_LEN  = 7,
    STATIC_KEYWORD_LEN    = 6,
    TYPEDEF_KEYWORD_LEN   = 7,
    STRUCT_KEYWORD_LEN    = 6,
    UNION_KEYWORD_LEN     = 5,
    ENUM_KEYWORD_LEN      = 4,
    DEFAULT_MAX_FUNCS     = 12,
    DEFAULT_MAX_PUB       = 8
};

#endif    // P101_MODULE_MAP_CONSTANTS_H
