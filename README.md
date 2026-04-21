# kwargs-C

A lightweight C library for parsing and validating typed key-value arguments,
inspired by Python's `**kwargs`.

---

## Table of Contents

- [Features](#features)
- [Build](#build)
- [Quick Start](#quick-start)
- [Input Formats](#input-formats)
- [API Reference](#api-reference)
  - [Initialization](#initialization)
  - [Reading Values](#reading-values)
  - [Supported Types](#supported-types)
  - [Error Handling](#error-handling)
  - [Options](#options)
- [Examples](#examples)
- [Notes](#notes)

---

## Features

- Parse arguments from `argc/argv` or any `char **` array
- Multiple input formats supported simultaneously:
  - `--key=value`
  - `key=value`
  - `--key value`
  - `key value`
- Automatic type conversion and validation for:
  - `int`
  - `double`
  - `bool` — accepts `true/false`, `yes/no`, `on/off`, `1/0`
  - `const char *`
- Configurable duplicate key policy
- Single generic macro API — no need to pick a different function per type
- Error details stored in `kwargs_t` with key name and message

---

## Build

No external dependencies. Requires a C11-compatible compiler.

```bash
gcc -Wall -Wextra -Werror -pedantic main.c kwargs.c -o app
```

---

## Quick Start

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "kwargs.h"

int main(int argc, char **argv) {
    kwargs_t kwargs;
    kwargs_options_t opts;

    kwargs_options_default(&opts);

    if (kwargs_init_from_args(&kwargs, argc, argv, &opts) != KW_OK) {
        fprintf(stderr, "parse error: %s\n", kwargs_error_string(&kwargs));
        kwargs_destroy(&kwargs);
        return 1;
    }

    int nb_rows;
    double threshold;
    bool dry_run;
    const char *filepath;

    KW_REQUIRE(&kwargs, "nb_rows",   &nb_rows);
    KW_REQUIRE(&kwargs, "threshold", &threshold);
    KW_REQUIRE(&kwargs, "dry_run",   &dry_run);
    KW_REQUIRE(&kwargs, "filepath",  &filepath);

    printf("nb_rows   = %d\n",   nb_rows);
    printf("threshold = %f\n",   threshold);
    printf("dry_run   = %s\n",   dry_run ? "true" : "false");
    printf("filepath  = %s\n",   filepath);

    kwargs_destroy(&kwargs);
    return 0;
}
```

```bash
./app --nb_rows=42 --threshold=0.8 --dry_run=true --filepath=data.csv
```

```
nb_rows   = 42
threshold = 0.800000
dry_run   = true
filepath  = data.csv
```

---

## Input Formats

All of the following are equivalent and can be mixed freely:

| Format         | Example                  |
|----------------|--------------------------|
| `--key=value`  | `--nb_rows=42`           |
| `key=value`    | `nb_rows=42`             |
| `--key value`  | `--nb_rows 42`           |
| `key value`    | `nb_rows 42`             |

Mixed example:

```bash
./app --nb_rows=42 threshold 0.8 --dry_run=true filepath data.csv
```

---

## API Reference

### Initialization

#### `kwargs_options_default`

```c
void kwargs_options_default(kwargs_options_t *opts);
```

Fills `opts` with default values:
- `dup_policy = KW_DUP_LAST`
- `strip_dashes = 1`

---

#### `kwargs_init_from_args`

```c
kw_error_t kwargs_init_from_args(
    kwargs_t *kwargs,
    int argc,
    char **argv,
    const kwargs_options_t *opts
);
```

Initializes `kwargs` from `argc/argv`.
`argv[0]` (the program name) is automatically skipped.

---

#### `kwargs_init_from_array`

```c
kw_error_t kwargs_init_from_array(
    kwargs_t *kwargs,
    size_t count,
    char **items,
    const kwargs_options_t *opts
);
```

Initializes `kwargs` from any `char **` array of `count` elements.

---

#### `kwargs_destroy`

```c
void kwargs_destroy(kwargs_t *kwargs);
```

Frees all internal memory allocated by `kwargs`.
Always call this when you are done using the `kwargs_t` instance.

---

### Reading Values

#### `KW_GET` — non-fatal

```c
kw_error_t KW_GET(kwargs_t *kwargs, const char *key, T *out);
```

Looks up `key`, converts the value to the type of `*out`, and writes the result.
The type is deduced automatically from the pointer passed as `out`.

Returns `KW_OK` on success, or an error code on failure.

```c
int nb_rows;

if (KW_GET(&kwargs, "nb_rows", &nb_rows) != KW_OK) {
    fprintf(stderr, "error on '%s': %s\n",
        kwargs.error_key,
        kwargs_error_string(&kwargs));
}
```

---

#### `KW_REQUIRE` — fatal

```c
KW_REQUIRE(kwargs_t *kwargs, const char *key, T *out);
```

Same as `KW_GET`, but on failure prints an error to `stderr` and calls
`exit(EXIT_FAILURE)`.

Use this when a missing or invalid argument is unrecoverable.

```c
int nb_rows;
KW_REQUIRE(&kwargs, "nb_rows", &nb_rows);
```

---

### Supported Types

The type passed to `KW_GET` / `KW_REQUIRE` is deduced from the pointer type of
`out`:

| Pointer type    | Expected format                         |
|-----------------|-----------------------------------------|
| `int *`         | Integer: `42`, `-7`                     |
| `double *`      | Float: `0.8`, `-3.14`                   |
| `bool *`        | `true/false`, `yes/no`, `on/off`, `1/0` |
| `const char **` | Any string                              |

---

### Error Handling

After any call to `KW_GET` or an `init` function, error details are available
on the `kwargs_t` struct:

```c
kwargs.error      // kw_error_t — error code
kwargs.error_key  // const char * — key that caused the error
```

```c
const char *kwargs_error_string(const kwargs_t *kwargs);
```

Returns a human-readable description of the current error.

#### Error Codes

| Code                         | Meaning                              |
|------------------------------|--------------------------------------|
| `KW_OK`                      | No error                             |
| `KW_ERR_NOT_FOUND`           | Key not found                        |
| `KW_ERR_INVALID_INT`         | Value cannot be parsed as `int`      |
| `KW_ERR_INVALID_DOUBLE`      | Value cannot be parsed as `double`   |
| `KW_ERR_INVALID_BOOL`        | Value cannot be parsed as `bool`     |
| `KW_ERR_UNSUPPORTED_TYPE`    | Type not supported by `KW_GET`       |
| `KW_ERR_PARSE_MISSING_VALUE` | Key present but has no value         |
| `KW_ERR_PARSE_EMPTY_KEY`     | Empty key found during parsing       |
| `KW_ERR_PARSE_DUPLICATE_KEY` | Duplicate key with `KW_DUP_ERROR`    |
| `KW_ERR_ALLOC`               | Memory allocation failure            |

---

### Options

```c
typedef struct {
    kw_dup_policy_t dup_policy;
    int strip_dashes;
} kwargs_options_t;
```

#### `dup_policy`

Controls behavior when the same key appears more than once in the input.

| Value          | Behavior                           |
|----------------|------------------------------------|
| `KW_DUP_FIRST` | Keep the first occurrence          |
| `KW_DUP_LAST`  | Keep the last occurrence (default) |
| `KW_DUP_ERROR` | Fail on duplicate key              |

Example with `KW_DUP_LAST` (default):

```bash
./app nb_rows=10 nb_rows=20 ...
# nb_rows = 20
```

Example with `KW_DUP_FIRST`:

```bash
./app nb_rows=10 nb_rows=20 ...
# nb_rows = 10
```

Example with `KW_DUP_ERROR`:

```bash
./app nb_rows=10 nb_rows=20 ...
# kwargs init error: key='nb_rows': duplicate key while parsing arguments
```

---

#### `strip_dashes`

| Value | Behavior                                                  |
|-------|-----------------------------------------------------------|
| `1`   | Strip leading `-` and `--` from keys (default)            |
| `0`   | Use keys as-is (`--nb_rows` is stored as `--nb_rows`)     |

---

## Examples

### From `argc/argv`

```c
kwargs_t kwargs;
kwargs_options_t opts;

kwargs_options_default(&opts);

if (kwargs_init_from_args(&kwargs, argc, argv, &opts) != KW_OK) {
    fprintf(stderr, "init error: %s\n", kwargs_error_string(&kwargs));
    kwargs_destroy(&kwargs);
    return 1;
}

int nb_rows;
KW_REQUIRE(&kwargs, "nb_rows", &nb_rows);

kwargs_destroy(&kwargs);
```

---

### From any `char **` array

```c
char *items[] = {
    "filepath", "data.csv",
    "nb_rows=42",
    "--threshold=0.75",
    "--dry_run", "true"
};

kwargs_t kwargs;
kwargs_options_t opts;

kwargs_options_default(&opts);

if (kwargs_init_from_array(
        &kwargs,
        sizeof(items) / sizeof(items[0]),
        items,
        &opts) != KW_OK) {
    fprintf(stderr, "init error: %s\n", kwargs_error_string(&kwargs));
    kwargs_destroy(&kwargs);
    return 1;
}

int nb_rows;
const char *filepath;

KW_REQUIRE(&kwargs, "nb_rows",  &nb_rows);
KW_REQUIRE(&kwargs, "filepath", &filepath);

kwargs_destroy(&kwargs);
```

---

### Non-fatal error handling

```c
int nb_rows;
double threshold;

if (KW_GET(&kwargs, "nb_rows", &nb_rows) != KW_OK) {
    fprintf(stderr, "warning: nb_rows not set, using default\n");
    nb_rows = 100;
}

if (KW_GET(&kwargs, "threshold", &threshold) != KW_OK) {
    fprintf(stderr, "warning: threshold not set, using default\n");
    threshold = 0.5;
}
```

---

## Notes

- `const char *` values returned by `KW_GET` / `KW_REQUIRE` point directly
  into the original input array. Do not free them, and do not modify the
  source array while `kwargs_t` is in use.
- Always call `kwargs_destroy` when done to avoid memory leaks.
- `KW_GET` and `KW_REQUIRE` require C11 `_Generic` support.
  Tested with `gcc` and `clang`.
- `strdup` is used internally. On non-POSIX systems (e.g. Windows with MSVC),
  you may need to replace it with a manual implementation.
