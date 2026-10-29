/*
============================================================================
Filename    : prog0.c
Author      : Your names goes here
SCIPER		: Your SCIPER numbers
============================================================================
*/

#include <stdio.h>
#include <stdlib.h>

#include "function.h"
#include "utility.h"

int main(int argc, char *argv[]) {
    int nrounds, size;

    /* Parse input arguments */
    if (argc != 3) {
        printf("Invalid input! Usage: ./prog0 <nrounds> <size>\n");
        return 1;
    } else {
        nrounds = atoi(argv[1]);
        size = atoi(argv[2]);
    }

    int *model = (int*) malloc(size * sizeof(int));
    int *result = (int*) calloc(size, sizeof(int));
    int *tmp = NULL;

    /* Initialise model */
    rand_gen generator = init_rand(0);
    for(int i = 0; i < size; i++) {
        model[i] = next_rand(generator) * size;
    }
    free_rand(generator);

    set_clock();

    for(int round = 0; round < nrounds; round++) {
        compute(model, result, size, size);

        for (int i = 0; i < size; i++) {
            model[i] += result[i];
            result[i] = 0;
        }
    }

    /* Output stats */
    double totaltime = elapsed_time();

    printf(
        "- Using 1 procs for %d iterations on %d size: %.3gs.\n",
        nrounds, size, totaltime
    );

    write_csv(model, 1, size, "model.csv");

    free(model);
    return 0;
}
