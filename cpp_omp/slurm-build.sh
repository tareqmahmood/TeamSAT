#!/usr/bin/env zsh
#SBATCH --job-name=Final
#SBATCH --partition=instruction
#SBATCH --time=00-00:03:00
#SBATCH --output=output.txt
#SBATCH --error=error.txt
#SBATCH --cpus-per-task=1 
#SBATCH --ntasks-per-node=1
#SBATCH --nodes=1

cd $SLURM_SUBMIT_DIR

g++ sat_solver.cpp -Wall -O3 -std=c++17 -o sat_solver.out -fopenmp
