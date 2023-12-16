#!/usr/bin/env zsh
#SBATCH --job-name=Final
#SBATCH --partition=instruction
#SBATCH --time=00-00:10:00
#SBATCH --output=output.txt
#SBATCH --error=error.txt
#SBATCH --cpus-per-task=20
#SBATCH --ntasks-per-node=1
#SBATCH --nodes=1


cnf_file=$1
n_threads=$2

VERSION="cpp_omp/$n_threads"

cd $SLURM_SUBMIT_DIR

BENCHMARK_DIR="../benchmark"
RESULTS_DIR="$BENCHMARK_DIR/results/$VERSION"

mkdir -p "$RESULTS_DIR" 

test_name=$(basename "$cnf_file" .cnf)

for (( t=1; t<=4; t++ ))
do
  ./sat_solver.out "$cnf_file" $n_threads >> "$RESULTS_DIR/$test_name.txt"
done
