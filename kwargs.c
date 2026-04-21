#include "kwargs.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const char *kwargs_find(kwargs_t *kwargs, const char *key) {
    size_t key_len = strlen(key);

    for (int i = 1; i < kwargs->argc; ++i) {
        const char *arg = kwargs->argv[i];

        if (strncmp(arg, "--", 2) != 0) {
            continue;
        }

        arg += 2;

        if (strncmp(arg, key, key_len) == 0 && arg[key_len] == '=') {
            return arg + key_len + 1;
        }
    }

    kwargs->error = KW_ERR_NOT_FOUND;
    kwargs->error_key = key;
    return NULL;
}

static int parse_int(const char *s, int *out) {
    char *end = NULL;
    long value;

    if (s == NULL || *s == '\0') {
        return 0;
    }

    errno = 0;
    value = strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0') {
        return 0;
    }

    if (value < INT_MIN || value > INT_MAX) {
        return 0;
    }

    *out = (int)value;
    return 1;
}

static int parse_double(const char *s, double *out) {
    char *end = NULL;
    double value;

    if (s == NULL || *s == '\0') {
        return 0;
    }

    errno = 0;
    value = strtod(s, &end);

    if (errno != 0 || end == s || *end != '\0') {
        return 0;
    }

    *out = value;
    return 1;
}
