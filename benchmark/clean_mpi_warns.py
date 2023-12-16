import glob

TEXT = 'tag_match.c'

for file in glob.glob('results/cpp_mpi/*/*.txt'):
    with open(file) as f:
        lines = f.readlines()
    lines = [line for line in lines if TEXT not in line]
    with open(file, 'w') as f:
        f.writelines(lines)
