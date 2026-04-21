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

static int parse_bool(const char *s, bool *out) {
    if (s == NULL) {
        return 0;
    }

    if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0 ||
        strcmp(s, "yes") == 0 || strcmp(s, "on") == 0) {
        *out = true;
        return 1;
    }

    if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0 ||
        strcmp(s, "no") == 0 || strcmp(s, "off") == 0) {
        *out = false;
        return 1;
    }

    return 0;
}

void kwargs_init(kwargs_t *kwargs, int argc, char **argv) {
    kwargs->argc = argc;
    kwargs->argv = argv;
    kwargs->error = KW_OK;
    kwargs->error_key = NULL;
}

kw_error_t kwargs_get(
    kwargs_t *kwargs,
    const char *key,
    kw_type_t type,
    void *out
) {
    const char *value = kwargs_find(kwargs, key);

    if (value == NULL) {
        return kwargs->error;
    }

    switch (type) {
        case KW_TYPE_INT:
            if (!parse_int(value, (int *)out)) {
                kwargs->error = KW_ERR_INVALID_INT;
                kwargs->error_key = key;
                return kwargs->error;
            }
            break;

        case KW_TYPE_DOUBLE:
            if (!parse_double(value, (double *)out)) {
                kwargs->error = KW_ERR_INVALID_DOUBLE;
                kwargs->error_key = key;
                return kwargs->error;
            }
            break;

        case KW_TYPE_BOOL:
            if (!parse_bool(value, (bool *)out)) {
                kwargs->error = KW_ERR_INVALID_BOOL;
                kwargs->error_key = key;
                return kwargs->error;
            }
            break;

        case KW_TYPE_STR:
            *(const char **)out = value;
            break;

        case KW_TYPE_INVALID:
        default:
            kwargs->error = KW_ERR_UNSUPPORTED_TYPE;
            kwargs->error_key = key;
            return kwargs->error;
    }

    kwargs->error = KW_OK;
    kwargs->error_key = NULL;
    return KW_OK;
}
