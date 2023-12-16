import slurm
import os

os.chdir("../cpp/")

for _ in range(4):
    slurm.sbatch('sbatch slurm-bench.sh')
