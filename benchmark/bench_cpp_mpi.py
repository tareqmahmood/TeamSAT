import slurm
import os
import glob

os.chdir("../cpp_mpi/")

test_cases = glob.glob('../test_cases/*.cnf')

for test_case in test_cases:
    for num_nodes in [3, 5, 7, 9, 11]:
        for t in range(4):
            slurm.sbatch(f'sbatch -N {num_nodes} slurm-bench.sh {test_case}')
