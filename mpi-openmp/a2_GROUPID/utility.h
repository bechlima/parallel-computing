/*
 ============================================================================
 Filename    : utility.h
 Author      : PARSA, EPFL
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#define MAX_VAL 1000

struct timeval start, end;

typedef struct rand_gen {
	unsigned short * seed;
	double (*rand_func) (struct rand_gen);
} rand_gen;

// Initializes the clock
void set_clock(){
	gettimeofday(&start, NULL);
}

// Returns time elapsed since set_clock() was called
double elapsed_time(){
	gettimeofday(&end, NULL);
	
	double elapsed = (end.tv_sec - start.tv_sec); 
	elapsed += (double)(end.tv_usec - start.tv_usec) / 1000000.0;
	return elapsed;
}

// Gives a random number between 0 and 1
double next_rand(rand_gen gen){	
	return erand48(gen.seed);
}	

// Initializes the random number generator
// It needs a thread id to generate different seeds for different threads
rand_gen init_rand(int tid){
	unsigned short * seed_array = malloc (sizeof (unsigned short) * 3);
	seed_array[0] = 5;
	seed_array[1] = 10;
	seed_array[2] = tid;

	rand_gen gen;
	gen.seed = seed_array;
	gen.rand_func = next_rand;

	return gen;
}

// Frees the memory allocated for the random number generator
void free_rand(rand_gen gen){
	free(gen.seed);
}

// Writes a matrix to a csv file
void write_csv(int *mat[], int rows, int cols, char *filename) {
    FILE *file = fopen(filename, "w");
    if(file == NULL) {
        printf("Error: Unable to open file %s\n", filename);
        exit(1);
    }

    for(int idx = 0; idx < rows; idx++) {
        for(int jdx = 0; jdx < cols; jdx++) {
            fprintf(file, "%d%s", mat[idx][jdx], (jdx < cols - 1) ? "," : "");
        }
        fprintf(file, "\n");
    }

    fclose(file);
}

// Display a matrix in the console
void display_matrix(int *mat[], int rows, int cols, char *matrixname) {
    printf("Displaying Matrix %s:\n", matrixname);
    for(int idx = 0; idx < rows; idx++) {
        for(int jdx = 0; jdx < cols; jdx++) {
            printf("%d ", mat[idx][jdx]);
        }
        printf("\n");
    }
}

// Initialize a matrix with random values
void init_mat(int *mat[], int rows, int cols, int seed) {
    rand_gen generator = init_rand(seed);
	for(int idx = 0; idx < rows; idx++) {
        mat[idx] = (int *)malloc(cols * sizeof(int));
        for (int jdx = 0; jdx < cols; jdx++) {
			if(seed == -1)
				mat[idx][jdx] = 0;
			else
                mat[idx][jdx] = next_rand(generator) * MAX_VAL;
        }
    }
	free_rand(generator);
}