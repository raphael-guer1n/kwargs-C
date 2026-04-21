#ifndef KWARGS_H
#define KWARGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Enums                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    KW_OK = 0,
    KW_ERR_NOT_FOUND,
    KW_ERR_INVALID_INT,
    KW_ERR_INVALID_DOUBLE,
    KW_ERR_INVALID_BOOL,
    KW_ERR_UNSUPPORTED_TYPE,
    KW_ERR_PARSE_MISSING_VALUE,
    KW_ERR_PARSE_EMPTY_KEY,
    KW_ERR_PARSE_DUPLICATE_KEY,
    KW_ERR_FORMAT_MISMATCH,
    KW_ERR_ALLOC
} kw_error_t;

typedef enum {
    KW_TYPE_INT = 0,
    KW_TYPE_DOUBLE,
    KW_TYPE_BOOL,
    KW_TYPE_STR,
    KW_TYPE_INVALID
} kw_type_t;

typedef enum {
    KW_DUP_FIRST = 0,
    KW_DUP_LAST,
    KW_DUP_ERROR
} kw_dup_policy_t;

typedef enum {
    KW_FMT_ANY = 0,  /* key=value, --key=value, key value, --key value  */
    KW_FMT_INLINE,   /* key=value or --key=value only                   */
    KW_FMT_SEPARATE, /* key value  or --key value  only                 */
    KW_FMT_FLAG,     /* --flag alone, no value, implicitly true          */
} kw_fmt_t;

/* ------------------------------------------------------------------ */
/* Structs                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    kw_dup_policy_t dup_policy;
    int             strip_dashes;
} kwargs_options_t;

typedef struct {
    const char *key;
    kw_fmt_t    fmt;
} kw_schema_entry_t;

typedef struct {
    char       *key;
    const char *value;
} kw_entry_t;

typedef struct {
    kw_entry_t  *entries;
    size_t       count;
    size_t       capacity;
    kw_error_t   error;
    const char  *error_key;
    kw_dup_policy_t dup_policy;
} kwargs_t;

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

void kwargs_options_default(kwargs_options_t *opts);

kw_error_t kwargs_init_from_args(
    kwargs_t                *kwargs,
    int                      argc,
    char                   **argv,
    const kwargs_options_t  *opts,
    const kw_schema_entry_t *schema,
    size_t                   schema_len
);

kw_error_t kwargs_init_from_array(
    kwargs_t                *kwargs,
    size_t                   count,
    char                   **items,
    const kwargs_options_t  *opts,
    const kw_schema_entry_t *schema,
    size_t                   schema_len
);

void kwargs_destroy(kwargs_t *kwargs);

kw_error_t kwargs_get(
    kwargs_t   *kwargs,
    const char *key,
    kw_type_t   type,
    void       *out
);

const char *kwargs_error_string(const kwargs_t *kwargs);

/* ------------------------------------------------------------------ */
/* Macros                                                              */
/* ------------------------------------------------------------------ */

#define KW_SCHEMA(...)                                                    \
    (const kw_schema_entry_t[]){ __VA_ARGS__ },                           \
    (sizeof((const kw_schema_entry_t[]){ __VA_ARGS__ }) /                 \
     sizeof(kw_schema_entry_t))

#define KW_TYPEOF_OUT(ptr)      \
    _Generic((ptr),             \
        int *:         KW_TYPE_INT,    \
        double *:      KW_TYPE_DOUBLE, \
        bool *:        KW_TYPE_BOOL,   \
        const char **: KW_TYPE_STR,    \
        char **:       KW_TYPE_STR,    \
        default:       KW_TYPE_INVALID \
    )

#define KW_GET(kwargs, key, out_ptr) \
    kwargs_get((kwargs), (key), KW_TYPEOF_OUT(out_ptr), (out_ptr))

#define KW_REQUIRE(kwargs, key, out_ptr)                            \
    do {                                                            \
        if (KW_GET((kwargs), (key), (out_ptr)) != KW_OK) {          \
            fprintf(stderr, "kwargs error: key='%s': %s\n",         \
                (kwargs)->error_key                                  \
                    ? (kwargs)->error_key                            \
                    : (key),                                         \
                kwargs_error_string((kwargs)));                     \
            exit(EXIT_FAILURE);                                      \
        }                                                           \
    } while (0)

#endif /* KWARGS_H */