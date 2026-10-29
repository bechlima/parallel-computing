/*
============================================================================
Filename    : progA.c
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
    if(argc != 3) {
        printf("Invalid input! Usage: ./progA <nrounds> <size>\n");
        return 1;
    } else {
        nrounds = atoi(argv[1]);
        size = atoi(argv[2]);
    }

    /* Get the number of processes */
    /* int nprocs = ??; */

    /* Initialise model */
    rand_gen generator = init_rand(0);
    int *model = (int*) malloc(size * sizeof(int));
    for(int i = 0; i < size; i++) {
        model[i] = next_rand(generator) * MAX_VAL;
    }
    free_rand(generator);

    set_clock();
    for(int round = 0; round < nrounds; round++) {
        /* Write code to train for N rounds */
    }

    /* Output stats */
    double totaltime = elapsed_time();
    printf("- Using %d procs for %d iterations on %d size: %.3gs.\n", nprocs, nrounds, size, totaltime);
    write_csv(&model, 1, size, "model.csv");
}