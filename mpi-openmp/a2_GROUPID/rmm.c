/*
============================================================================
Filename    : rmm.c
Author      : <FILL IN NAME>
SCIPER      : <FILL IN SCIPER>
Description : MPI-parallel reduced matrix multiply (RMM).

  C is (M/2) x (K/2) where
      C[i][j] = sum_{aoff in {0,1}} sum_{boff in {0,1}} sum_{k=0..N-1}
                  A[2i+aoff][k] * B[k][2j+boff]

  Parallelization: rank 0 broadcasts B and scatters row-pairs of A across
  ranks; each rank computes its slab of C; rank 0 gathers the result.
  Flattening to row-contiguous form is done outside the timed region
  (allowed by the spec).
============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "utility.h"

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if(argc != 5) {
        if(rank == 0) printf("Usage: %s <M> <N> <K> <0|1>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    /* Step 1 (UNCHANGED): read M, N, K, debug */
    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);
    int debug = atoi(argv[4]);

    if(M % 2 != 0 || N % 2 != 0 || K % 2 != 0) {
        if(rank == 0) printf("M, N and K must be even\n");
        MPI_Finalize();
        return 1;
    }

    /* Step 2 (UNCHANGED on rank 0): generate A, B; allocate C */
    int *matA[M];
    int *matB[N];
    int *matC[M/2];
    if(rank == 0) {
        init_mat(matA, M, N, 0);
        init_mat(matB, N, K, 1);
        init_mat(matC, M/2, K/2, -1);

        if(debug) {
            display_matrix(matA, M, N, "A");
            display_matrix(matB, N, K, "B");
        }
    }

    /* ----- UNTIMED: flatten matrices into contiguous row-major buffers -----
     * Allowed by the spec: flattening is not timed. */
    int *A_flat = NULL;       /* M x N row-major   (rank 0 only)         */
    int *B_flat = NULL;       /* N x K row-major   (all ranks)           */
    int *C_flat = NULL;       /* (M/2) x (K/2)     (rank 0 only)         */

    B_flat = (int*) malloc((size_t)N * K * sizeof(int));
    if(rank == 0) {
        A_flat = (int*) malloc((size_t)M * N * sizeof(int));
        for(int i = 0; i < M; i++) {
            memcpy(A_flat + (size_t)i * N, matA[i], N * sizeof(int));
        }
        for(int i = 0; i < N; i++) {
            memcpy(B_flat + (size_t)i * K, matB[i], K * sizeof(int));
        }
        C_flat = (int*) malloc((size_t)(M/2) * (K/2) * sizeof(int));
    }

    /* Partition the M/2 output row-pairs of C across ranks (Scatterv).
     * rank r gets row-pairs [start_pair, start_pair + my_pairs).
     * Each row-pair needs 2 rows of A (= 2*N ints) and produces 1 row of C
     * (= K/2 ints).                                                          */
    int total_pairs = M / 2;
    int base_pairs  = total_pairs / nprocs;
    int extra_pairs = total_pairs - base_pairs * nprocs;

    int *pairs_per_rank = (int*) malloc(nprocs * sizeof(int));
    int *pair_displs    = (int*) malloc(nprocs * sizeof(int));
    {
        int acc = 0;
        for(int r = 0; r < nprocs; r++) {
            pairs_per_rank[r] = base_pairs + (r < extra_pairs ? 1 : 0);
            pair_displs[r]    = acc;
            acc += pairs_per_rank[r];
        }
    }

    /* Build sendcounts/displs in element units for A (each pair = 2*N ints)
     * and for C (each pair = K/2 ints).                                    */
    int *A_sendcounts = (int*) malloc(nprocs * sizeof(int));
    int *A_displs_e   = (int*) malloc(nprocs * sizeof(int));
    int *C_recvcounts = (int*) malloc(nprocs * sizeof(int));
    int *C_displs_e   = (int*) malloc(nprocs * sizeof(int));
    for(int r = 0; r < nprocs; r++) {
        A_sendcounts[r] = pairs_per_rank[r] * 2 * N;
        A_displs_e[r]   = pair_displs[r]    * 2 * N;
        C_recvcounts[r] = pairs_per_rank[r] * (K/2);
        C_displs_e[r]   = pair_displs[r]    * (K/2);
    }

    int my_pairs = pairs_per_rank[rank];
    int *A_local = (int*) malloc((size_t)my_pairs * 2 * N * sizeof(int));
    int *C_local = (int*) malloc((size_t)my_pairs * (K/2) * sizeof(int));

    /* Step 3 (TIMED): all of the actual MPI compute */
    if(rank == 0) printf("Starting Computation...\n");
    MPI_Barrier(MPI_COMM_WORLD);
    set_clock();

    /* (a) broadcast B to all ranks */
    MPI_Bcast(B_flat, N * K, MPI_INT, 0, MPI_COMM_WORLD);

    /* (b) scatter the A row-pairs */
    MPI_Scatterv(A_flat, A_sendcounts, A_displs_e, MPI_INT,
                 A_local, my_pairs * 2 * N, MPI_INT,
                 0, MPI_COMM_WORLD);

    /* (c) local compute: same kernel as the sequential skeleton, but
     *     reordered to (p, aoff, k, j, boff) so the inner loops stream
     *     B in row-major order and write to C with unit stride.  No
     *     algebraic transformation of the kernel.                      */
    for(int p = 0; p < my_pairs; p++) {
        int *Crow = C_local + (size_t)p * (K/2);
        memset(Crow, 0, (K/2) * sizeof(int));
        for(int aoff = 0; aoff < 2; aoff++) {
            const int *Arow = A_local + (size_t)(p * 2 + aoff) * N;
            for(int k = 0; k < N; k++) {
                int a = Arow[k];
                const int *Brow = B_flat + (size_t)k * K;
                for(int j = 0; j < K/2; j++) {
                    Crow[j] += a * Brow[2*j];
                    Crow[j] += a * Brow[2*j + 1];
                }
            }
        }
    }

    /* (d) gather C slabs back to rank 0 */
    MPI_Gatherv(C_local, my_pairs * (K/2), MPI_INT,
                C_flat,  C_recvcounts, C_displs_e, MPI_INT,
                0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double totaltime = elapsed_time();

    /* ----- UNTIMED: copy contiguous C_flat back into row-pointer matC ----- */
    if(rank == 0) {
        for(int i = 0; i < M/2; i++) {
            memcpy(matC[i], C_flat + (size_t)i * (K/2), (K/2) * sizeof(int));
        }
    }

    /* Step 4 (UNCHANGED): write CSV */
    if(rank == 0) {
        printf("Computation Done!\n");
        if(debug) display_matrix(matC, M/2, K/2, "C");
        printf("- Using %d procs: matC computed in %.4gs.\n", nprocs, totaltime);
        write_csv(matC, M/2, K/2, "matC.csv");
    }

    /* Cleanup */
    free(A_local); free(C_local);
    free(pairs_per_rank); free(pair_displs);
    free(A_sendcounts);   free(A_displs_e);
    free(C_recvcounts);   free(C_displs_e);
    free(B_flat);
    if(rank == 0) {
        free(A_flat); free(C_flat);
        for(int i = 0; i < M;     i++) free(matA[i]);
        for(int i = 0; i < N;     i++) free(matB[i]);
        for(int i = 0; i < M/2;   i++) free(matC[i]);
    }

    MPI_Finalize();
    return 0;
}
