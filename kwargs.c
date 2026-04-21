#include "kwargs.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

static void kwargs_reset(kwargs_t *kwargs) {
    kwargs->entries = NULL;
    kwargs->count = 0;
    kwargs->capacity = 0;
    kwargs->error = KW_OK;
    kwargs->error_key = NULL;
    kwargs->dup_policy = KW_DUP_LAST;
}

static int ensure_capacity(kwargs_t *kwargs, size_t needed) {
    if (kwargs->capacity >= needed) {
        return 1;
    }

    size_t new_capacity = kwargs->capacity == 0 ? 8 : kwargs->capacity * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    kw_entry_t *new_entries =
        realloc(kwargs->entries, new_capacity * sizeof(*new_entries));
    if (new_entries == NULL) {
        kwargs->error = KW_ERR_ALLOC;
        kwargs->error_key = NULL;
        return 0;
    }

    kwargs->entries = new_entries;
    kwargs->capacity = new_capacity;
    return 1;
}

static const char *strip_prefix_dashes(const char *s, int strip_dashes) {
    if (!strip_dashes || s == NULL) {
        return s;
    }

    while (*s == '-') {
        ++s;
    }

    return s;
}

static int find_entry_index(const kwargs_t *kwargs, const char *key) {
    for (size_t i = 0; i < kwargs->count; ++i) {
        if (strcmp(kwargs->entries[i].key, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int add_entry(kwargs_t *kwargs, const char *key, const char *value) {
    int idx;

    if (key == NULL || *key == '\0') {
        kwargs->error = KW_ERR_PARSE_EMPTY_KEY;
        kwargs->error_key = key;
        return 0;
    }

    idx = find_entry_index(kwargs, key);
    if (idx >= 0) {
        switch (kwargs->dup_policy) {
            case KW_DUP_FIRST:
                return 1;

            case KW_DUP_LAST:
                kwargs->entries[idx].value = value;
                return 1;

            case KW_DUP_ERROR:
                kwargs->error = KW_ERR_PARSE_DUPLICATE_KEY;
                kwargs->error_key = kwargs->entries[idx].key;
                return 0;
        }
    }

    if (!ensure_capacity(kwargs, kwargs->count + 1)) {
        return 0;
    }

    char *key_copy = strdup(key);
    if (key_copy == NULL) {
        kwargs->error = KW_ERR_ALLOC;
        kwargs->error_key = NULL;
        return 0;
    }

    kwargs->entries[kwargs->count].key = key_copy;
    kwargs->entries[kwargs->count].value = value;
    kwargs->count++;
    return 1;
}

static int split_inline_kv(
    const char *token,
    int strip_dashes,
    const char **out_key_start,
    size_t *out_key_len,
    const char **out_value
) {
    const char *eq;

    token = strip_prefix_dashes(token, strip_dashes);
    eq = strchr(token, '=');

    if (eq == NULL) {
        return 0;
    }

    *out_key_start = token;
    *out_key_len = (size_t)(eq - token);
    *out_value = eq + 1;
    return 1;
}

static kw_error_t kwargs_init_from_tokens(
    kwargs_t *kwargs,
    size_t count,
    char **items,
    const kwargs_options_t *opts
) {
    size_t i = 0;

    kwargs_reset(kwargs);
    kwargs->dup_policy = opts ? opts->dup_policy : KW_DUP_LAST;

    while (i < count) {
        const char *token = items[i];
        const char *key_start = NULL;
        size_t key_len = 0;
        const char *value = NULL;
        int strip = opts ? opts->strip_dashes : 1;

        if (token == NULL || *token == '\0') {
            ++i;
            continue;
        }

        if (split_inline_kv(token, strip, &key_start, &key_len, &value)) {
            char key_buf[key_len + 1];
            memcpy(key_buf, key_start, key_len);
            key_buf[key_len] = '\0';

            if (!add_entry(kwargs, key_buf, value)) {
                return kwargs->error;
            }
            ++i;
            continue;
        }

        const char *key = strip_prefix_dashes(token, strip);

        if (key == NULL || *key == '\0') {
            kwargs->error = KW_ERR_PARSE_EMPTY_KEY;
            kwargs->error_key = token;
            return kwargs->error;
        }

        if (i + 1 >= count) {
            kwargs->error = KW_ERR_PARSE_MISSING_VALUE;
            kwargs->error_key = key;
            return kwargs->error;
        }

        value = items[i + 1];
        if (value == NULL) {
            kwargs->error = KW_ERR_PARSE_MISSING_VALUE;
            kwargs->error_key = key;
            return kwargs->error;
        }

        if (!add_entry(kwargs, key, value)) {
            return kwargs->error;
        }

        i += 2;
    }

    kwargs->error = KW_OK;
    kwargs->error_key = NULL;
    return KW_OK;
}

static const char *kwargs_find(kwargs_t *kwargs, const char *key) {
    int idx = find_entry_index(kwargs, key);
    if (idx < 0) {
        kwargs->error = KW_ERR_NOT_FOUND;
        kwargs->error_key = key;
        return NULL;
    }

    return kwargs->entries[idx].value;
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

void kwargs_options_default(kwargs_options_t *opts) {
    if (opts == NULL) {
        return;
    }

    opts->dup_policy = KW_DUP_LAST;
    opts->strip_dashes = 1;
}

kw_error_t kwargs_init_from_args(
    kwargs_t *kwargs,
    int argc,
    char **argv,
    const kwargs_options_t *opts
) {
    if (argc <= 1) {
        kwargs_reset(kwargs);
        return KW_OK;
    }

    return kwargs_init_from_tokens(
        kwargs,
        (size_t)(argc - 1),
        argv + 1,
        opts
    );
}

kw_error_t kwargs_init_from_array(
    kwargs_t *kwargs,
    size_t count,
    char **items,
    const kwargs_options_t *opts
) {
    return kwargs_init_from_tokens(kwargs, count, items, opts);
}

void kwargs_destroy(kwargs_t *kwargs) {
    if (kwargs == NULL) {
        return;
    }

    for (size_t i = 0; i < kwargs->count; ++i) {
        free(kwargs->entries[i].key);
    }

    free(kwargs->entries);
    kwargs->entries = NULL;
    kwargs->count = 0;
    kwargs->capacity = 0;
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

const char *kwargs_error_string(const kwargs_t *kwargs) {
    switch (kwargs->error) {
        case KW_OK:
            return "no error";
        case KW_ERR_NOT_FOUND:
            return "key not found";
        case KW_ERR_INVALID_INT:
            return "invalid int";
        case KW_ERR_INVALID_DOUBLE:
            return "invalid double";
        case KW_ERR_INVALID_BOOL:
            return "invalid bool";
        case KW_ERR_UNSUPPORTED_TYPE:
            return "unsupported type";
        case KW_ERR_PARSE_MISSING_VALUE:
            return "missing value while parsing arguments";
        case KW_ERR_PARSE_EMPTY_KEY:
            return "empty key while parsing arguments";
        case KW_ERR_PARSE_DUPLICATE_KEY:
            return "duplicate key while parsing arguments";
        case KW_ERR_ALLOC:
            return "memory allocation failure";
        default:
            return "unknown error";
    }
}