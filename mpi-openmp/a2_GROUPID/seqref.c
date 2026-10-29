/* Sequential reference for progA-D correctness checks.
 * Same algorithm as prog0.c but uses MAX_VAL init (matches progA-D skeletons). */
#include <stdio.h>
#include <stdlib.h>
#include "function.h"
#include "utility.h"

int main(int argc, char *argv[]) {
    if(argc != 3) { printf("Usage: %s <nrounds> <size>\n", argv[0]); return 1; }
    int nrounds = atoi(argv[1]);
    int size    = atoi(argv[2]);

    int *model  = (int*) malloc(size * sizeof(int));
    int *result = (int*) calloc(size, sizeof(int));

    rand_gen generator = init_rand(0);
    for(int i = 0; i < size; i++) {
        model[i] = next_rand(generator) * MAX_VAL;
    }
    free_rand(generator);

    for(int round = 0; round < nrounds; round++) {
        compute(model, result, size, size);
        for(int i = 0; i < size; i++) {
            model[i] += result[i];
            result[i] = 0;
        }
    }

    write_csv(&model, 1, size, "model.csv");
    free(model); free(result);
    return 0;
}
