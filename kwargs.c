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
