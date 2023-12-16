#!/usr/bin/env zsh
#SBATCH --job-name=Final
#SBATCH --partition=instruction
#SBATCH --time=00-00:03:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --output=output.txt
#SBATCH --error=error.txt

cd $SLURM_SUBMIT_DIR

./sat_solver.out ../test_cases/bmc-2.cnf