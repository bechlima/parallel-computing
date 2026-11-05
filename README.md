# Parallel Matrix Computation with MPI and OpenMP

Distributed-memory and hybrid parallel programs in C, run on EPFL's SCITAS clusters with Slurm. They were written for EPFL CS-302 (Parallelism and Concurrency), spring 2026, by Imad Bechlaghem and Florian Della Santa.

## Programs

| File | What it does |
|---|---|
| `progA.c` | Distributed data aggregation over MPI, used as the baseline |
| `progB.c` | Aggregation with **blocking** point-to-point sends, batching B1/B2 elements per message |
| `progC.c` | Same as B with **non-blocking** `MPI_Isend`/`MPI_Irecv`, overlapping communication and compute |
| `progD.c` | Aggregation with **collectives**: `MPI_Scatterv` to workers and `MPI_Reduce` back |
| `rmm.c` | MPI reduced matrix multiply: broadcast B, scatter row-pairs of A, local compute, `MPI_Gatherv` the result |
| `rmm_openmp_mpi.c` | Hybrid version: MPI across nodes, OpenMP threads across each rank's rows |
| `rmm_seq.c`, `seqref.c` | Sequential references used for correctness checks |
| `verify.sh` | Slurm job that builds everything and diffs outputs against the references over a sweep of process counts, sizes and batch parameters |

**Reduced matrix multiply:** `C` is `(M/2) × (K/2)`, and each output sums the 2×2 neighbourhood of products of rows of `A` and columns of `B`:
`C[i][j] = Σ_{a,b∈{0,1}} Σ_k A[2i+a][k] · B[k][2j+b]`.

## Build and run

```bash
mpicc -O2 -o rmm rmm.c
mpicc -O2 -fopenmp -o rmm_openmp_mpi rmm_openmp_mpi.c
srun -n 8 ./rmm <M> <N> <K> 0
srun -n 4 ./rmm_openmp_mpi <M> <N> <K> 0 <threads_per_rank>
```

On a Slurm cluster, `sbatch verify.sh` runs the full correctness sweep. The `#SBATCH` account and QoS lines are set for the EPFL course allocation, so change them for another cluster.

`function.h` and `utility.h` are course-provided helpers (© PARSA, EPFL).
