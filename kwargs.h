#ifndef KWARGS_H
#define KWARGS_H

#include <stdbool.h>


typedef struct {
    int argc;
    char **argv;
    kw_error_t error;
    const char *error_key;
} kwargs_t;


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
