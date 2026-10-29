/*
============================================================================
Filename    : progD.c
Author      : <FILL IN NAME>
SCIPER      : <FILL IN SCIPER>
Description : Distributed data aggregation using MPI collectives:
              Scatterv (server -> workers) and Reduce (workers -> server).
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

    if(argc != 3) {
        if(rank == 0) printf("Usage: ./progD <nrounds> <size>\n");
        MPI_Finalize();
        return 1;
    }
    nrounds = atoi(argv[1]);
    size    = atoi(argv[2]);

    if(nprocs < 2) {
        if(rank == 0) printf("This program requires at least 2 processes.\n");
        MPI_Finalize();
        return 1;
    }

    int nworkers   = nprocs - 1;
    int base_chunk = size / nworkers;
    int remainder  = size - base_chunk * nworkers;

    /* Initialise model identically on every rank */
    rand_gen generator = init_rand(0);
    int *model = (int*) malloc(size * sizeof(int));
    for(int i = 0; i < size; i++) {
        model[i] = next_rand(generator) * MAX_VAL;
    }
    free_rand(generator);

    /* --- Build sendcounts/displs for Scatterv ---
     * rank 0 -> 0 elements; rank w (1..P-1) -> base_chunk (last gets +remainder) */
    int *sendcounts = (int*) malloc(nprocs * sizeof(int));
    int *displs     = (int*) malloc(nprocs * sizeof(int));
    sendcounts[0] = 0;
    displs[0]     = 0;
    for(int w = 1; w < nprocs; w++) {
        sendcounts[w] = (w == nprocs - 1) ? (base_chunk + remainder) : base_chunk;
        displs[w]     = (w - 1) * base_chunk;
    }
    int my_chunk = sendcounts[rank];

    int *recv_chunk = (my_chunk > 0) ? (int*) malloc(my_chunk * sizeof(int)) : NULL;
    int *worker_out = (int*) malloc(size * sizeof(int));   /* on every rank */
    int *result     = NULL;
    if(rank == 0) result = (int*) malloc(size * sizeof(int));

    MPI_Barrier(MPI_COMM_WORLD);
    set_clock();

    for(int round = 0; round < nrounds; round++) {

        /* Step 1: scatter chunks of model from rank 0 to workers */
        MPI_Scatterv(model, sendcounts, displs, MPI_INT,
                     recv_chunk, my_chunk, MPI_INT,
                     0, MPI_COMM_WORLD);

        /* Step 2: workers run compute(); rank 0 contributes a zero buffer */
        memset(worker_out, 0, size * sizeof(int));
        if(rank != 0) {
            compute(recv_chunk, worker_out, my_chunk, size);
        }

        /* Step 3: element-wise sum-reduce all worker outputs into result on rank 0 */
        MPI_Reduce(worker_out, result, size, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

        /* Rank 0 updates the model */
        if(rank == 0) {
            for(int i = 0; i < size; i++) {
                model[i] += result[i];
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
    free(sendcounts); free(displs);
    free(worker_out);
    if(recv_chunk) free(recv_chunk);
    if(rank == 0) free(result);

    MPI_Finalize();
    return 0;
}
