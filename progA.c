/*
============================================================================
Filename    : progA.c
Author      : <Imad Bechlaghem, Florian Della Santa>
SCIPER      : <375401, >
============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "function.h"
#include "utility.h"

int main(int argc, char *argv[]) {
    int nrounds, size;

    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    /* Parse input arguments */
    if(argc != 3) {
        if(rank == 0) printf("Invalid input! Usage: ./progA <nrounds> <size>\n");
        MPI_Finalize();
        return 1;
    }
    nrounds = atoi(argv[1]);
    size = atoi(argv[2]);

    if(nprocs < 2) {
        if(rank == 0) printf("This program requires at least 2 processes.\n");
        MPI_Finalize();
        return 1;
    }

    int nworkers = nprocs - 1;
    int base_chunk = size / nworkers;          /* size/(P-1)        */
    int remainder  = size - base_chunk * nworkers;  /* given to last worker */

    /* Initialise model identically on every rank
     * (same seed -> same array; avoids an extra broadcast at startup) */
    rand_gen generator = init_rand(0);
    int *model = (int*) malloc(size * sizeof(int));
    for(int i = 0; i < size; i++) {
        model[i] = next_rand(generator) * MAX_VAL;
    }
    free_rand(generator);

    /* Per-round scratch space */
    int *recv_chunk = NULL;        /* worker: holds its slice of the model */
    int *worker_out = NULL;        /* worker: full-size output histogram   */
    int *result_buf = NULL;        /* rank 0: holds incoming worker output */

    if(rank == 0) {
        result_buf = (int*) malloc(size * sizeof(int));
    } else {
        int my_chunk = (rank == nprocs - 1) ? (base_chunk + remainder) : base_chunk;
        recv_chunk = (int*) malloc(my_chunk * sizeof(int));
        worker_out = (int*) malloc(size * sizeof(int));
    }

    MPI_Barrier(MPI_COMM_WORLD);
    set_clock();

    for(int round = 0; round < nrounds; round++) {

        if(rank == 0) {
            /* Step 1: send each worker its chunk, ONE ELEMENT AT A TIME */
            for(int w = 1; w < nprocs; w++) {
                int start = (w - 1) * base_chunk;
                int len   = (w == nprocs - 1) ? (base_chunk + remainder) : base_chunk;
                for(int i = 0; i < len; i++) {
                    MPI_Send(&model[start + i], 1, MPI_INT, w, 0, MPI_COMM_WORLD);
                }
            }

            /* Step 3: receive each worker's full output and accumulate */
            for(int w = 1; w < nprocs; w++) {
                for(int i = 0; i < size; i++) {
                    MPI_Recv(&result_buf[i], 1, MPI_INT, w, 1,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
                for(int i = 0; i < size; i++) {
                    model[i] += result_buf[i];
                }
            }

        } else {
            int my_chunk = (rank == nprocs - 1) ? (base_chunk + remainder) : base_chunk;

            /* Step 1: receive chunk one element at a time */
            for(int i = 0; i < my_chunk; i++) {
                MPI_Recv(&recv_chunk[i], 1, MPI_INT, 0, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            /* Step 2: run compute() on my local chunk */
            memset(worker_out, 0, size * sizeof(int));
            compute(recv_chunk, worker_out, my_chunk, size);

            /* Send full output array back, one element at a time */
            for(int i = 0; i < size; i++) {
                MPI_Send(&worker_out[i], 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double totaltime = elapsed_time();

    if(rank == 0) {
        printf("- Using %d procs for %d iterations on %d size: %.3gs.\n",
               nprocs, nrounds, size, totaltime);
        write_csv(&model, 1, size, "model.csv");
    }

    free(model);
    if(rank == 0) free(result_buf);
    else { free(recv_chunk); free(worker_out); }

    MPI_Finalize();
    return 0;
}
