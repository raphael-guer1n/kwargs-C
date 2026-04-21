#ifndef KWARGS_H
#define KWARGS_H

#include <stdbool.h>


typedef struct {
    int argc;
    char **argv;
    kw_error_t error;
    const char *error_key;
} kwargs_t;

#endif
