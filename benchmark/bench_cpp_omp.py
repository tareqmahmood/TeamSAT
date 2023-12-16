import slurm
import os
import glob

os.chdir("../cpp_omp/")

test_cases = glob.glob('../test_cases/*.cnf')

for test_case in test_cases:
    for n_threads in [2, 4, 8, 12, 16]:
        slurm.sbatch(f'sbatch slurm-bench.sh {test_case} {n_threads}')
