/*
============================================================================
Filename    : progB.c
Author      : <FILL IN NAME>
SCIPER      : <FILL IN SCIPER>
Description : Distributed data aggregation with blocking, point-to-point
              communication, transmitting B1 / B2 elements per send.
============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "function.h"
#include "utility.h"

int main(int argc, char *argv[]) {
    int nrounds, size, B1, B2;

    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if(argc != 5) {
        if(rank == 0) printf("Usage: ./progB <nrounds> <size> <B1> <B2>\n");
        MPI_Finalize();
        return 1;
    }
    nrounds = atoi(argv[1]);
    size    = atoi(argv[2]);
    B1      = atoi(argv[3]);
    B2      = atoi(argv[4]);

    if(nprocs < 2) {
        if(rank == 0) printf("This program requires at least 2 processes.\n");
        MPI_Finalize();
        return 1;
    }
    if(B1 <= 0 || B2 <= 0) {
        if(rank == 0) printf("B1 and B2 must be positive.\n");
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

    int *recv_chunk = NULL;
    int *worker_out = NULL;
    int *result_buf = NULL;

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
            /* Step 1: send each worker its chunk in B1-sized pieces */
            for(int w = 1; w < nprocs; w++) {
                int start = (w - 1) * base_chunk;
                int len   = (w == nprocs - 1) ? (base_chunk + remainder) : base_chunk;
                for(int off = 0; off < len; off += B1) {
                    int n = (off + B1 <= len) ? B1 : (len - off);
                    MPI_Send(&model[start + off], n, MPI_INT, w, 0, MPI_COMM_WORLD);
                }
            }

            /* Step 3: receive each worker's full output in B2-sized pieces, then accumulate */
            for(int w = 1; w < nprocs; w++) {
                for(int off = 0; off < size; off += B2) {
                    int n = (off + B2 <= size) ? B2 : (size - off);
                    MPI_Recv(&result_buf[off], n, MPI_INT, w, 1,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
                for(int i = 0; i < size; i++) {
                    model[i] += result_buf[i];
                }
            }

        } else {
            int my_chunk = (rank == nprocs - 1) ? (base_chunk + remainder) : base_chunk;

            for(int off = 0; off < my_chunk; off += B1) {
                int n = (off + B1 <= my_chunk) ? B1 : (my_chunk - off);
                MPI_Recv(&recv_chunk[off], n, MPI_INT, 0, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            memset(worker_out, 0, size * sizeof(int));
            compute(recv_chunk, worker_out, my_chunk, size);

            for(int off = 0; off < size; off += B2) {
                int n = (off + B2 <= size) ? B2 : (size - off);
                MPI_Send(&worker_out[off], n, MPI_INT, 0, 1, MPI_COMM_WORLD);
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
