#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "kwargs.h"

static void my_function(kwargs_t *kwargs) {
    int nb_rows;
    double threshold;
    bool dry_run;
    const char *filepath;

    KW_REQUIRE(kwargs, "nb_rows", &nb_rows);
    KW_REQUIRE(kwargs, "threshold", &threshold);
    KW_REQUIRE(kwargs, "dry_run", &dry_run);
    KW_REQUIRE(kwargs, "filepath", &filepath);

    printf("nb_rows   = %d\n", nb_rows);
    printf("threshold = %f\n", threshold);
    printf("dry_run   = %s\n", dry_run ? "true" : "false");
    printf("filepath  = %s\n", filepath);
}

int main(int argc, char **argv) {
    kwargs_t kwargs;

    kwargs_init(&kwargs, argc, argv);
    my_function(&kwargs);

    return 0;
}
