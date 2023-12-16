#!/usr/bin/env zsh
#SBATCH --job-name=Final
#SBATCH --partition=instruction
#SBATCH --time=00-00:03:00
#SBATCH --output=output.txt
#SBATCH --error=error.txt
#SBATCH --cpus-per-task=10
#SBATCH --ntasks-per-node=1
#SBATCH --nodes=1

cd $SLURM_SUBMIT_DIR

./sat_solver.out ../test_cases/bmc-2.cnf 10