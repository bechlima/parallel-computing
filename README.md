# Accelerating Matrix Computations on GPUs and Clusters

EPFL parallel computing coursework (2026), written in C and CUDA, using MPI, OpenMP and Slurm.

- `mpi-openmp/`: sequential, OpenMP and hybrid MPI+OpenMP implementations of matrix workloads (`rmm*.c`, `prog*.c`), with a verification script. `a2_GROUPID/` is the grouped submission; the top-level files are working versions.
- `cuda/`: GPU kernels for matrix computation (`rmm.cu`, `assignment3.cu`), plus a Makefile and a Slurm job script (`execute.sh`).

## Build
Needs an MPI implementation with OpenMP support (e.g. `mpicc -fopenmp`) and the CUDA toolkit (`make` in `cuda/`). The scripts target EPFL's SCITAS clusters, so they may need adjusting elsewhere.

Course-supplied scaffolding remains the property of the course staff.
