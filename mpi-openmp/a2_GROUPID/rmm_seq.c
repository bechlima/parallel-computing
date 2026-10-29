/* Sequential RMM reference for rmm.c / rmm_openmp_mpi.c correctness checks.
 * Uses the unmodified skeleton kernel from the handout. */
#include <stdio.h>
#include <stdlib.h>
#include "utility.h"

int main(int argc, char *argv[]) {
    if(argc != 5) { printf("Usage: %s <M> <N> <K> <0|1>\n", argv[0]); return 1; }
    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);
    int debug = atoi(argv[4]);

    if(M%2 || N%2 || K%2) { printf("M, N, K must be even\n"); return 1; }

    int *matA[M];
    int *matB[N];
    int *matC[M/2];

    init_mat(matA, M, N, 0);
    init_mat(matB, N, K, 1);
    init_mat(matC, M/2, K/2, -1);

    for(int idx = 0; idx < M/2; idx++) {
        for(int jdx = 0; jdx < K/2; jdx++) {
            matC[idx][jdx] = 0;
            for(int aoff = 0; aoff < 2; aoff++) {
                for(int boff = 0; boff < 2; boff++) {
                    for(int kdx = 0; kdx < N; kdx++) {
                        matC[idx][jdx] += matA[idx*2 + aoff][kdx]
                                        * matB[kdx][jdx*2 + boff];
                    }
                }
            }
        }
    }

    if(debug) display_matrix(matC, M/2, K/2, "C");
    write_csv(matC, M/2, K/2, "matC.csv");

    for(int i = 0; i < M;   i++) free(matA[i]);
    for(int i = 0; i < N;   i++) free(matB[i]);
    for(int i = 0; i < M/2; i++) free(matC[i]);
    return 0;
}
