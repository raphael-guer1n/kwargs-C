#ifndef KWARGS_H
#define KWARGS_H

#include <stdbool.h>


typedef struct {
    int argc;
    char **argv;
    kw_error_t error;
    const char *error_key;
} kwargs_t;

void kwargs_init(kwargs_t *kwargs, int argc, char **argv);

kw_error_t kwargs_get(
    kwargs_t *kwargs,
    const char *key,
    kw_type_t type,
    void *out
);

const char *kwargs_error_string(const kwargs_t *kwargs);

#define KW_TYPEOF_OUT(ptr) _Generic((ptr), \
    int *: KW_TYPE_INT, \
    double *: KW_TYPE_DOUBLE, \
    bool *: KW_TYPE_BOOL, \
    const char **: KW_TYPE_STR, \
    char **: KW_TYPE_STR, \
    default: KW_TYPE_INVALID \
)

#define KW_GET(kwargs, key, out_ptr) \
    kwargs_get((kwargs), (key), KW_TYPEOF_OUT(out_ptr), (out_ptr))

#define KW_REQUIRE(kwargs, key, out_ptr)                                  \
    do {                                                                  \
        if (KW_GET((kwargs), (key), (out_ptr)) != KW_OK) {                \
            fprintf(stderr, "kwargs error: key='%s': %s\n",               \
                (kwargs)->error_key ? (kwargs)->error_key : (key),        \
                kwargs_error_string((kwargs)));                           \
            exit(EXIT_FAILURE);                                            \
        }                                                                 \
    } while (0)

#endif
