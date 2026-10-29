/*
============================================================================
Filename    : progC.c
Author      : <FILL IN NAME>
SCIPER      : <FILL IN SCIPER>
Description : Distributed data aggregation with non-blocking, point-to-point
              communication, transmitting B1 / B2 elements per request.
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
        if(rank == 0) printf("Usage: ./progC <nrounds> <size> <B1> <B2>\n");
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
    int *result_all = NULL;   /* rank 0: full size-S buffer per worker */

    if(rank == 0) {
        /* one full output buffer per worker so we can post all Irecvs at once */
        result_all = (int*) malloc((size_t)size * nworkers * sizeof(int));
    } else {
        int my_chunk = (rank == nprocs - 1) ? (base_chunk + remainder) : base_chunk;
        recv_chunk = (int*) malloc(my_chunk * sizeof(int));
        worker_out = (int*) malloc(size * sizeof(int));
    }

    /* Number of B1/B2-sized requests that fit per worker / per output */
    int max_pieces_send_to_worker = 0;
    {
        int last_len = base_chunk + remainder;
        int n_other  = (base_chunk + B1 - 1) / B1;
        int n_last   = (last_len   + B1 - 1) / B1;
        max_pieces_send_to_worker = (n_other > n_last) ? n_other : n_last;
    }
    int pieces_back = (size + B2 - 1) / B2;

    /* Pre-allocate request arrays */
    MPI_Request *reqs_send_chunks = NULL;  /* rank 0: distribute   */
    MPI_Request *reqs_recv_outs   = NULL;  /* rank 0: collect      */
    MPI_Request *reqs_recv_chunks = NULL;  /* worker: receive own chunk */
    MPI_Request *reqs_send_out    = NULL;  /* worker: send back    */

    if(rank == 0) {
        reqs_send_chunks = (MPI_Request*) malloc(
            (size_t)max_pieces_send_to_worker * nworkers * sizeof(MPI_Request));
        reqs_recv_outs = (MPI_Request*) malloc(
            (size_t)pieces_back * nworkers * sizeof(MPI_Request));
    } else {
        int my_chunk = (rank == nprocs - 1) ? (base_chunk + remainder) : base_chunk;
        int my_pieces = (my_chunk + B1 - 1) / B1;
        reqs_recv_chunks = (MPI_Request*) malloc(my_pieces * sizeof(MPI_Request));
        reqs_send_out    = (MPI_Request*) malloc(pieces_back * sizeof(MPI_Request));
    }

    MPI_Barrier(MPI_COMM_WORLD);
    set_clock();

    for(int round = 0; round < nrounds; round++) {

        if(rank == 0) {
            /* Step 1: post all Isends to all workers */
            int rs_idx = 0;
            for(int w = 1; w < nprocs; w++) {
                int start = (w - 1) * base_chunk;
                int len   = (w == nprocs - 1) ? (base_chunk + remainder) : base_chunk;
                for(int off = 0; off < len; off += B1) {
                    int n = (off + B1 <= len) ? B1 : (len - off);
                    MPI_Isend(&model[start + off], n, MPI_INT, w, 0,
                              MPI_COMM_WORLD, &reqs_send_chunks[rs_idx++]);
                }
            }

            /* Post all Irecvs for worker outputs (full size-S per worker) */
            int rr_idx = 0;
            for(int w = 1; w < nprocs; w++) {
                int *buf = result_all + (w - 1) * (size_t)size;
                for(int off = 0; off < size; off += B2) {
                    int n = (off + B2 <= size) ? B2 : (size - off);
                    MPI_Irecv(&buf[off], n, MPI_INT, w, 1,
                              MPI_COMM_WORLD, &reqs_recv_outs[rr_idx++]);
                }
            }

            /* Wait for everything to finish */
            MPI_Waitall(rs_idx, reqs_send_chunks, MPI_STATUSES_IGNORE);
            MPI_Waitall(rr_idx, reqs_recv_outs,   MPI_STATUSES_IGNORE);

            /* Accumulate */
            for(int w = 0; w < nworkers; w++) {
                int *buf = result_all + w * (size_t)size;
                for(int i = 0; i < size; i++) {
                    model[i] += buf[i];
                }
            }

        } else {
            int my_chunk = (rank == nprocs - 1) ? (base_chunk + remainder) : base_chunk;

            int rc_idx = 0;
            for(int off = 0; off < my_chunk; off += B1) {
                int n = (off + B1 <= my_chunk) ? B1 : (my_chunk - off);
                MPI_Irecv(&recv_chunk[off], n, MPI_INT, 0, 0,
                          MPI_COMM_WORLD, &reqs_recv_chunks[rc_idx++]);
            }
            MPI_Waitall(rc_idx, reqs_recv_chunks, MPI_STATUSES_IGNORE);

            memset(worker_out, 0, size * sizeof(int));
            compute(recv_chunk, worker_out, my_chunk, size);

            int ss_idx = 0;
            for(int off = 0; off < size; off += B2) {
                int n = (off + B2 <= size) ? B2 : (size - off);
                MPI_Isend(&worker_out[off], n, MPI_INT, 0, 1,
                          MPI_COMM_WORLD, &reqs_send_out[ss_idx++]);
            }
            MPI_Waitall(ss_idx, reqs_send_out, MPI_STATUSES_IGNORE);
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
    if(rank == 0) {
        free(result_all);
        free(reqs_send_chunks);
        free(reqs_recv_outs);
    } else {
        free(recv_chunk); free(worker_out);
        free(reqs_recv_chunks); free(reqs_send_out);
    }

    MPI_Finalize();
    return 0;
}
