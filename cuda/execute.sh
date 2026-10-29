#!/bin/bash
#SBATCH --chdir /scratch/izar/<username>
#SBATCH --partition=gpu
#SBATCH --qos=cs-302
#SBATCH --gres=gpu:1
#SBATCH --nodes=1
#SBATCH --time=1:0:0
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=1
#SBATCH --mem 1G
#SBATCH --account cs-302

module load gcc cuda

echo STARTING AT `date`
make all

./assignment3 1024 1024 1024 0
echo FINISHED at `date`