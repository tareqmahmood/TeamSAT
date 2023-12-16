#!/usr/bin/env zsh
#SBATCH --job-name=Final
#SBATCH --partition=instruction
#SBATCH --time=00-00:03:00
#SBATCH --output=output.txt
#SBATCH --error=error.txt

cnf_file=$1

module load mpi/openmpi/4.1.4

VERSION="cpp_mpi/$SLURM_NNODES"

cd $SLURM_SUBMIT_DIR

BENCHMARK_DIR="../benchmark"
TEST_CASES_DIR="../test_cases"
RESULTS_DIR="$BENCHMARK_DIR/results/$VERSION"

mkdir -p "$RESULTS_DIR" 

test_name=$(basename "$cnf_file" .cnf)

srun -n $SLURM_NNODES --cpu-bind=none "./sat_solver.out" "$cnf_file" >> "$RESULTS_DIR/$test_name.txt"
