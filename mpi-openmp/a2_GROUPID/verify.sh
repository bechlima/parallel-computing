#!/usr/bin/env bash
#SBATCH --account cs-302
#SBATCH --qos cs-302
#SBATCH --partition academic
#SBATCH --nodes 1
#SBATCH --ntasks-per-node 8
#SBATCH --cpus-per-task 1
#SBATCH --mem 8G
#SBATCH --time 00:15:00
#SBATCH --job-name verify
#SBATCH --output verify-%j.out

# Correctness sweep for A2.
# Compiles all programs + sequential references, then diffs output CSVs against
# the references across many configurations. Prints PASS/FAIL per test.
#
# Run from the directory containing the .c files and headers:
#     sbatch verify.sh

set -u

module purge
module load gcc
module load openmpi

PASS=0
FAIL=0

# ---------------------------------------------------------------------------
# Compile everything
# ---------------------------------------------------------------------------
echo "==> Compiling"
mpicc -O2 -o progA            progA.c            || { echo "BUILD FAIL: progA";            exit 1; }
mpicc -O2 -o progB            progB.c            || { echo "BUILD FAIL: progB";            exit 1; }
mpicc -O2 -o progC            progC.c            || { echo "BUILD FAIL: progC";            exit 1; }
mpicc -O2 -o progD            progD.c            || { echo "BUILD FAIL: progD";            exit 1; }
mpicc -O2 -o rmm              rmm.c              || { echo "BUILD FAIL: rmm";              exit 1; }
mpicc -O2 -fopenmp -o rmm_openmp_mpi rmm_openmp_mpi.c || { echo "BUILD FAIL: rmm_openmp_mpi"; exit 1; }
gcc   -O2 -o seqref           seqref.c           || { echo "BUILD FAIL: seqref";           exit 1; }
gcc   -O2 -o rmm_seq          rmm_seq.c          || { echo "BUILD FAIL: rmm_seq";          exit 1; }
echo "==> Compile OK"
echo

# ---------------------------------------------------------------------------
# Helper: run a test and update PASS/FAIL counts
# ---------------------------------------------------------------------------
check() {
    local label="$1" ref="$2" got="$3"
    if diff -q "$ref" "$got" > /dev/null 2>&1; then
        echo "  PASS  $label"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  $label"
        FAIL=$((FAIL + 1))
        # show first mismatch for debugging
        diff "$ref" "$got" | head -5
    fi
}

# ---------------------------------------------------------------------------
# Part 1: progA / progB / progC / progD
# ---------------------------------------------------------------------------
echo "==> Part 1 correctness"

for cfg in "5 100" "10 1000" "5 10000"; do
    read -r N S <<< "$cfg"

    # build the reference
    ./seqref "$N" "$S" > /dev/null
    cp model.csv ref_part1.csv

    for NP in 2 4 8; do
        # progA
        srun -n "$NP" ./progA "$N" "$S" > /dev/null 2>&1
        check "progA  np=$NP  N=$N  S=$S" ref_part1.csv model.csv

        # progD
        srun -n "$NP" ./progD "$N" "$S" > /dev/null 2>&1
        check "progD  np=$NP  N=$N  S=$S" ref_part1.csv model.csv

        # progB and progC at a couple of chunk sizes
        for B1 in 1 100; do
            for B2 in 1 100; do
                srun -n "$NP" ./progB "$N" "$S" "$B1" "$B2" > /dev/null 2>&1
                check "progB  np=$NP  N=$N  S=$S  B1=$B1  B2=$B2" ref_part1.csv model.csv

                srun -n "$NP" ./progC "$N" "$S" "$B1" "$B2" > /dev/null 2>&1
                check "progC  np=$NP  N=$N  S=$S  B1=$B1  B2=$B2" ref_part1.csv model.csv
            done
        done
    done
done

echo
# ---------------------------------------------------------------------------
# Part 2: rmm
# ---------------------------------------------------------------------------
echo "==> Part 2 (rmm) correctness"

for cfg in "16 16 16" "32 64 48" "128 128 128" "256 256 256"; do
    read -r M N K <<< "$cfg"

    ./rmm_seq "$M" "$N" "$K" 0 > /dev/null
    cp matC.csv ref_part2.csv

    for NP in 1 2 4 8; do
        srun -n "$NP" ./rmm "$M" "$N" "$K" 0 > /dev/null 2>&1
        check "rmm  np=$NP  ${M}x${N}x${K}" ref_part2.csv matC.csv
    done
done

echo
# ---------------------------------------------------------------------------
# Part 2: rmm_openmp_mpi (hybrid)
# ---------------------------------------------------------------------------
echo "==> Part 2 (rmm_openmp_mpi) correctness"

for cfg in "32 32 32" "128 128 128"; do
    read -r M N K <<< "$cfg"

    ./rmm_seq "$M" "$N" "$K" 0 > /dev/null
    cp matC.csv ref_part2.csv

    for NP in 1 2 4; do
        for T in 1 2; do
            export OMP_NUM_THREADS=$T
            srun -n "$NP" --cpus-per-task=$T ./rmm_openmp_mpi "$M" "$N" "$K" 0 "$T" > /dev/null 2>&1
            check "rmm_openmp_mpi  np=$NP  t=$T  ${M}x${N}x${K}" ref_part2.csv matC.csv
        done
    done
done

echo
echo "============================================="
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================="

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
