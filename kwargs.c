#include "kwargs.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static void kwargs_reset(kwargs_t *kwargs) {
    kwargs->entries   = NULL;
    kwargs->count     = 0;
    kwargs->capacity  = 0;
    kwargs->error     = KW_OK;
    kwargs->error_key = NULL;
    kwargs->dup_policy = KW_DUP_LAST;
}

static int ensure_capacity(kwargs_t *kwargs, size_t needed) {
    if (kwargs->capacity >= needed) return 1;

    size_t new_cap = kwargs->capacity == 0 ? 8 : kwargs->capacity * 2;
    while (new_cap < needed) new_cap *= 2;

    kw_entry_t *p =
        realloc(kwargs->entries, new_cap * sizeof(*p));
    if (p == NULL) {
        kwargs->error     = KW_ERR_ALLOC;
        kwargs->error_key = NULL;
        return 0;
    }

    kwargs->entries  = p;
    kwargs->capacity = new_cap;
    return 1;
}

static const char *strip_prefix_dashes(const char *s, int strip) {
    if (!strip || s == NULL) return s;
    while (*s == '-') ++s;
    return s;
}

static int find_entry_index(const kwargs_t *kwargs, const char *key) {
    for (size_t i = 0; i < kwargs->count; ++i)
        if (strcmp(kwargs->entries[i].key, key) == 0) return (int)i;
    return -1;
}

static const kw_schema_entry_t *find_schema(
    const kw_schema_entry_t *schema,
    size_t                   schema_len,
    const char              *key
) {
    if (schema == NULL) return NULL;
    for (size_t i = 0; i < schema_len; ++i)
        if (strcmp(schema[i].key, key) == 0) return &schema[i];
    return NULL;
}

static int add_entry(
    kwargs_t   *kwargs,
    const char *key,
    const char *value
) {
    if (key == NULL || *key == '\0') {
        kwargs->error     = KW_ERR_PARSE_EMPTY_KEY;
        kwargs->error_key = key;
        return 0;
    }

    int idx = find_entry_index(kwargs, key);
    if (idx >= 0) {
        switch (kwargs->dup_policy) {
            case KW_DUP_FIRST: return 1;
            case KW_DUP_LAST:
                kwargs->entries[idx].value = value;
                return 1;
            case KW_DUP_ERROR:
                kwargs->error     = KW_ERR_PARSE_DUPLICATE_KEY;
                kwargs->error_key = kwargs->entries[idx].key;
                return 0;
        }
    }

    if (!ensure_capacity(kwargs, kwargs->count + 1)) return 0;

    char *key_copy = strdup(key);
    if (key_copy == NULL) {
        kwargs->error     = KW_ERR_ALLOC;
        kwargs->error_key = NULL;
        return 0;
    }

    kwargs->entries[kwargs->count].key   = key_copy;
    kwargs->entries[kwargs->count].value = value;
    kwargs->count++;
    return 1;
}

/* returns 1 if token contains '=' (after stripping dashes) */
static int token_is_inline(const char *token, int strip) {
    const char *s = strip_prefix_dashes(token, strip);
    return strchr(s, '=') != NULL;
}

/* returns 1 if token looks like it should be a key, not a bare value:
   - starts with '-' (after optional strip it still has content), or
   - contains '=' (inline key=value)
   Used by KW_FMT_ANY to avoid eating the next key as a value. */
static int token_looks_like_key(const char *token, int strip) {
    if (token == NULL || *token == '\0') return 0;
    if (token_is_inline(token, strip))   return 1;
    /* starts with at least one dash */
    if (*token == '-') return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Core parser                                                         */
/* ------------------------------------------------------------------ */

static kw_error_t kwargs_init_from_tokens(
    kwargs_t                *kwargs,
    size_t                   count,
    char                   **items,
    const kwargs_options_t  *opts,
    const kw_schema_entry_t *schema,
    size_t                   schema_len
) {
    kwargs_reset(kwargs);
    kwargs->dup_policy = opts ? opts->dup_policy : KW_DUP_LAST;

    int strip = opts ? opts->strip_dashes : 1;
    size_t i  = 0;

    while (i < count) {
        const char *token = items[i];

        if (token == NULL || *token == '\0') { ++i; continue; }

        /* ---- resolve key name (stripped) for schema lookup ---- */
        const char *stripped = strip_prefix_dashes(token, strip);

        /* ---- detect inline key=value ---- */
        const char *eq = strchr(stripped, '=');

        if (eq != NULL) {
            /* inline format: (--)?key=value */
            size_t key_len = (size_t)(eq - stripped);
            char   key_buf[key_len + 1];
            memcpy(key_buf, stripped, key_len);
            key_buf[key_len] = '\0';

            const kw_schema_entry_t *se =
                find_schema(schema, schema_len, key_buf);
            kw_fmt_t fmt = se ? se->fmt : KW_FMT_ANY;

            if (fmt == KW_FMT_SEPARATE) {
                kwargs->error     = KW_ERR_FORMAT_MISMATCH;
                kwargs->error_key = se->key;
                return kwargs->error;
            }
            if (fmt == KW_FMT_FLAG) {
                kwargs->error     = KW_ERR_FORMAT_MISMATCH;
                kwargs->error_key = se->key;
                return kwargs->error;
            }

            if (!add_entry(kwargs, key_buf, eq + 1)) return kwargs->error;
            ++i;
            continue;
        }

        /* ---- no '=': key is the whole stripped token ---- */
        const char *key = stripped;

        if (key == NULL || *key == '\0') {
            kwargs->error     = KW_ERR_PARSE_EMPTY_KEY;
            kwargs->error_key = token;
            return kwargs->error;
        }

        const kw_schema_entry_t *se =
            find_schema(schema, schema_len, key);
        kw_fmt_t fmt = se ? se->fmt : KW_FMT_ANY;

        /* ---- FLAG: presence alone means true ---- */
        if (fmt == KW_FMT_FLAG) {
            if (!add_entry(kwargs, key, "true")) return kwargs->error;
            ++i;
            continue;
        }

        /* ---- INLINE only: must have '=', which we don't → error ---- */
        if (fmt == KW_FMT_INLINE) {
            kwargs->error     = KW_ERR_FORMAT_MISMATCH;
            kwargs->error_key = se->key;
            return kwargs->error;
        }

        /* ---- SEPARATE or ANY: consume next token as value ---- */
        if (fmt == KW_FMT_ANY) {
            /* guard: don't eat a key as a value */
            if (i + 1 < count &&
                token_looks_like_key(items[i + 1], strip)) {
                kwargs->error     = KW_ERR_PARSE_MISSING_VALUE;
                kwargs->error_key = key;
                return kwargs->error;
            }
        }

        if (i + 1 >= count || items[i + 1] == NULL) {
            kwargs->error     = KW_ERR_PARSE_MISSING_VALUE;
            kwargs->error_key = key;
            return kwargs->error;
        }

        /* SEPARATE: next token must not be inline */
        if (fmt == KW_FMT_SEPARATE &&
            token_is_inline(items[i + 1], strip)) {
            kwargs->error     = KW_ERR_FORMAT_MISMATCH;
            kwargs->error_key = se->key;
            return kwargs->error;
        }

        if (!add_entry(kwargs, key, items[i + 1])) return kwargs->error;
        i += 2;
    }

    kwargs->error     = KW_OK;
    kwargs->error_key = NULL;
    return KW_OK;
}

/* ------------------------------------------------------------------ */
/* Type parsers                                                        */
/* ------------------------------------------------------------------ */

static int parse_int(const char *s, int *out) {
    if (s == NULL || *s == '\0') return 0;
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return 0;
    if (v < INT_MIN || v > INT_MAX)             return 0;
    *out = (int)v;
    return 1;
}

static int parse_double(const char *s, double *out) {
    if (s == NULL || *s == '\0') return 0;
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0') return 0;
    *out = v;
    return 1;
}

static int parse_bool(const char *s, bool *out) {
    if (s == NULL) return 0;
    if (strcmp(s, "1")    == 0 || strcmp(s, "true")  == 0 ||
        strcmp(s, "yes")  == 0 || strcmp(s, "on")    == 0) {
        *out = true;  return 1;
    }
    if (strcmp(s, "0")    == 0 || strcmp(s, "false") == 0 ||
        strcmp(s, "no")   == 0 || strcmp(s, "off")   == 0) {
        *out = false; return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void kwargs_options_default(kwargs_options_t *opts) {
    if (opts == NULL) return;
    opts->dup_policy  = KW_DUP_LAST;
    opts->strip_dashes = 1;
}

kw_error_t kwargs_init_from_args(
    kwargs_t                *kwargs,
    int                      argc,
    char                   **argv,
    const kwargs_options_t  *opts,
    const kw_schema_entry_t *schema,
    size_t                   schema_len
) {
    if (argc <= 1) { kwargs_reset(kwargs); return KW_OK; }
    return kwargs_init_from_tokens(
        kwargs, (size_t)(argc - 1), argv + 1, opts, schema, schema_len
    );
}

kw_error_t kwargs_init_from_array(
    kwargs_t                *kwargs,
    size_t                   count,
    char                   **items,
    const kwargs_options_t  *opts,
    const kw_schema_entry_t *schema,
    size_t                   schema_len
) {
    return kwargs_init_from_tokens(
        kwargs, count, items, opts, schema, schema_len
    );
}

void kwargs_destroy(kwargs_t *kwargs) {
    if (kwargs == NULL) return;
    for (size_t i = 0; i < kwargs->count; ++i)
        free(kwargs->entries[i].key);
    free(kwargs->entries);
    kwargs->entries   = NULL;
    kwargs->count     = 0;
    kwargs->capacity  = 0;
    kwargs->error     = KW_OK;
    kwargs->error_key = NULL;
}

kw_error_t kwargs_get(
    kwargs_t   *kwargs,
    const char *key,
    kw_type_t   type,
    void       *out
) {
    int idx = find_entry_index(kwargs, key);
    if (idx < 0) {
        kwargs->error     = KW_ERR_NOT_FOUND;
        kwargs->error_key = key;
        return kwargs->error;
    }

    const char *value = kwargs->entries[idx].value;

    switch (type) {
        case KW_TYPE_INT:
            if (!parse_int(value, (int *)out)) {
                kwargs->error     = KW_ERR_INVALID_INT;
                kwargs->error_key = key;
                return kwargs->error;
            }
            break;
        case KW_TYPE_DOUBLE:
            if (!parse_double(value, (double *)out)) {
                kwargs->error     = KW_ERR_INVALID_DOUBLE;
                kwargs->error_key = key;
                return kwargs->error;
            }
            break;
        case KW_TYPE_BOOL:
            if (!parse_bool(value, (bool *)out)) {
                kwargs->error     = KW_ERR_INVALID_BOOL;
                kwargs->error_key = key;
                return kwargs->error;
            }
            break;
        case KW_TYPE_STR:
            *(const char **)out = value;
            break;
        case KW_TYPE_INVALID:
        default:
            kwargs->error     = KW_ERR_UNSUPPORTED_TYPE;
            kwargs->error_key = key;
            return kwargs->error;
    }

    kwargs->error     = KW_OK;
    kwargs->error_key = NULL;
    return KW_OK;
}

const char *kwargs_error_string(const kwargs_t *kwargs) {
    switch (kwargs->error) {
        case KW_OK:                      return "no error";
        case KW_ERR_NOT_FOUND:           return "key not found";
        case KW_ERR_INVALID_INT:         return "invalid int";
        case KW_ERR_INVALID_DOUBLE:      return "invalid double";
        case KW_ERR_INVALID_BOOL:        return "invalid bool";
        case KW_ERR_UNSUPPORTED_TYPE:    return "unsupported type";
        case KW_ERR_PARSE_MISSING_VALUE: return "missing value";
        case KW_ERR_PARSE_EMPTY_KEY:     return "empty key";
        case KW_ERR_PARSE_DUPLICATE_KEY: return "duplicate key";
        case KW_ERR_FORMAT_MISMATCH:     return "format mismatch";
        case KW_ERR_ALLOC:               return "memory allocation failure";
        default:                         return "unknown error";
    }
}