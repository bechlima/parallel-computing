/*
============================================================================
Filename    : assignment3.c
Author      : PARSA, EPFL
============================================================================
*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sys/time.h>
#include <cuda_runtime.h>
using namespace std;
#include "utility.h"

void rmm_cpu(int *matA, int *matB, int *matC, int M, int N, int K);
void rmm_gpu(int *matA, int *matB, int *matC, int M, int N, int K);

int main (int argc, const char *argv[]) {
    if(argc != 5) {
        printf("Usage: %s <M> <N> <K> <0|1>\n", argv[0]);
        return 1;
    }
    
    /* Step 1: Read the values of M, N and K from the command line arguments. */
    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);
    int debug = atoi(argv[4]);

    if(M % 2 != 0 || N % 2 != 0 || K % 2 != 0) {
        printf("M, N and K must be even\n");
        return 1;
    }

    /* Step 2: Generates and initializes matrices A and B with random values. */
    int *matA, *matB, *matC;
    matA = (int *) malloc(M * N * sizeof(int));
    matB = (int *) malloc(N * K * sizeof(int));
    matC = (int *) malloc((M/2) * (K/2) * sizeof(int));

    init_mat(matA, M, N, 0);
    init_mat(matB, N, K, 1);        
    init_mat(matC, M/2, K/2, -1);   // -1 indicates that matrix is initialized with 0s

    if(debug) {
        display_matrix(matA, M, N, "A");
        display_matrix(matB, N, K, "B");
    }

    /* Reset Device */
    cudaDeviceReset();

    /* Start Timer */
    set_clock();

    /* Use either the CPU or the GPU functions */
    // rmm_cpu(matA, matB, matC, M, N, K);  // Uncomment this line to use CPU function
    rmm_gpu(matA, matB, matC, M, N, K);

    /* Stop Timer */
    double totaltime = elapsed_time();

    /* Print output matrix if debug */
    if(debug)
        display_matrix(matC, M/2, K/2, "C");

    /* Report time required for the entire program */
    cout << "Total time taken: " << setprecision(4) << totaltime << "s" << endl;

    /* Write the output matrix to a file */
    write_csv(matC, M/2, K/2, "matC.csv");
    
    /* Free allocated memory */
    free(matA);
    free(matB);
    free(matC);

    return 0;
}
