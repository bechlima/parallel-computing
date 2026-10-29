/*
============================================================================
Filename    : function.h
Author      : PARSA, EPFL
============================================================================
*/

#define ITERATIONS 25

// Integer Hash function (https://github.com/skeeto/hash-prospector)
static __always_inline int triple32(int x) {
    for (int i = 0; i < ITERATIONS; i++) {
        x ^= x >> 17;
        x *= 0xed5ad4bb;
        x ^= x >> 11;
        x *= 0xac4c1b51;
        x ^= x >> 15;
        x *= 0x31848bab;
        x ^= x >> 14;
    }

    return x;
}

void compute (int *in, int *out, int chunk_size, int size){
    for (int i = 0; i < chunk_size; i++) {
        int key = in[i];
        int hash_val = ((unsigned int)triple32(key)) % size;

        out[hash_val]++;
    }
}
