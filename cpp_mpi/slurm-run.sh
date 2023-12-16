#!/usr/bin/env zsh
#SBATCH --job-name=Final
#SBATCH --partition=instruction
#SBATCH --time=00-00:03:00
#SBATCH --output=output.txt
#SBATCH --error=error.txt
#SBATCH --cpus-per-task=1 
#SBATCH --ntasks-per-node=1
#SBATCH --nodes=7

module load mpi/openmpi/4.1.4

cd $SLURM_SUBMIT_DIR

srun -n 7 --cpu-bind=none ./sat_solver.out ../test_cases/bmc-2.cnf