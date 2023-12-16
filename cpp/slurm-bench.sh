#!/usr/bin/env zsh
#SBATCH --job-name=Final
#SBATCH --partition=instruction
#SBATCH --time=00-00:03:00
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --output=output.txt
#SBATCH --error=error.txt

VERSION="cpp"

cd $SLURM_SUBMIT_DIR

BENCHMARK_DIR="../benchmark"
TEST_CASES_DIR="../test_cases"
RESULTS_DIR="$BENCHMARK_DIR/results/$VERSION"

mkdir -p "$RESULTS_DIR"

# g++ sat_solver.cpp -Wall -O3 -std=c++17 -o sat_solver.out

if [ $? -eq 0 ]; then
    echo "Compilation successful. Running sat_solver.out for each test case..."
    # Loop through each .cnf file in test_cases
    for cnf_file in "$TEST_CASES_DIR"/*.cnf; do
        # Extract the file name without extension
        file_name=$(basename "$cnf_file" .cnf)

        # Run sat_solver.out for the current test case
        "./sat_solver.out" "$cnf_file" >> "$RESULTS_DIR/$file_name.txt"

        # Check if execution was successful
        if [ $? -eq 0 ]; then
            echo "Test case $file_name completed successfully."
        else
            echo "Error running sat_solver.out for test case $file_name."
        fi
    done

    echo "All test cases completed."

else
    echo "Compilation failed. Please check your code for errors."
fi


# ./sat_solver.out ../test_cases/bmc-7.cnf
