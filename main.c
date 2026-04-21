#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "kwargs.h"

static void print_init_error(const kwargs_t *kwargs) {
    fprintf(
        stderr,
        "kwargs init error: key='%s': %s\n",
        kwargs->error_key ? kwargs->error_key : "(none)",
        kwargs_error_string(kwargs)
    );
}

static void demo_from_argv(int argc, char **argv) {
    kwargs_t         kwargs;
    kwargs_options_t opts;

    kwargs_options_default(&opts);

    /*
     * nb_rows    → --nb_rows 42        (SEPARATE)
     * threshold  → --threshold=0.8     (INLINE)
     * dry_run    → --dry_run           (FLAG)
     * filepath   → any format          (ANY)
     */
    if (kwargs_init_from_args(
            &kwargs, argc, argv, &opts,
            KW_SCHEMA(
                { "nb_rows",   KW_FMT_SEPARATE },
                { "threshold", KW_FMT_INLINE   },
                { "dry_run",   KW_FMT_FLAG     },
                { "filepath",  KW_FMT_ANY      }
            )) != KW_OK) {
        print_init_error(&kwargs);
        kwargs_destroy(&kwargs);
        exit(EXIT_FAILURE);
    }

    int         nb_rows;
    double      threshold;
    bool        dry_run;
    const char *filepath;

    KW_REQUIRE(&kwargs, "nb_rows",   &nb_rows);
    KW_REQUIRE(&kwargs, "threshold", &threshold);
    KW_REQUIRE(&kwargs, "dry_run",   &dry_run);
    KW_REQUIRE(&kwargs, "filepath",  &filepath);

    printf("[argv]\n");
    printf("nb_rows   = %d\n",              nb_rows);
    printf("threshold = %f\n",              threshold);
    printf("dry_run   = %s\n",              dry_run ? "true" : "false");
    printf("filepath  = %s\n",              filepath);

    kwargs_destroy(&kwargs);
}

static void demo_from_array(void) {
    kwargs_t         kwargs;
    kwargs_options_t opts;

    char *items[] = {
        "filepath",  "data.csv",
        "--nb_rows", "42",
        "--threshold=0.75",
        "--dry_run",
    };

    kwargs_options_default(&opts);

    if (kwargs_init_from_array(
            &kwargs,
            sizeof(items) / sizeof(items[0]),
            items,
            &opts,
            KW_SCHEMA(
                { "nb_rows",   KW_FMT_SEPARATE },
                { "threshold", KW_FMT_INLINE   },
                { "dry_run",   KW_FMT_FLAG     },
                { "filepath",  KW_FMT_ANY      }
            )) != KW_OK) {
        print_init_error(&kwargs);
        kwargs_destroy(&kwargs);
        exit(EXIT_FAILURE);
    }

    int         nb_rows;
    double      threshold;
    bool        dry_run;
    const char *filepath;

    KW_REQUIRE(&kwargs, "nb_rows",   &nb_rows);
    KW_REQUIRE(&kwargs, "threshold", &threshold);
    KW_REQUIRE(&kwargs, "dry_run",   &dry_run);
    KW_REQUIRE(&kwargs, "filepath",  &filepath);

    printf("[array]\n");
    printf("nb_rows   = %d\n",              nb_rows);
    printf("threshold = %f\n",              threshold);
    printf("dry_run   = %s\n",              dry_run ? "true" : "false");
    printf("filepath  = %s\n",              filepath);

    kwargs_destroy(&kwargs);
}

int main(int argc, char **argv) {
    demo_from_argv(argc, argv);
    demo_from_array();
    return 0;
}