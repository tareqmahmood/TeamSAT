import pandas as pd 
import glob

version = 'cpp_mpi'

n_nodes = [3, 5, 7, 9, 11]

results = []
results_columns = ['test_name']
for n in n_nodes:
    results_columns.append(f'mean_ms_{n}_nodes')

test_cases = pd.read_csv('test_cases.csv')



for i, row in test_cases.iterrows():
    record = [row.test_name]
    for n in n_nodes:
        result_file = f'results/{version}/{n}/{row.test_name}.txt'
        times = []
        with open(result_file) as file:
            for line in file:
                line = line.strip()
                line = line.split()
                if line[0] == 'Time' and len(line) == 2:
                    times.append(float(line[1]))
        if len(times) > 0:
            mean_ms = round(sum(times) / len(times), 3)
            record.append(mean_ms)
    
    results.append(record)

results = pd.DataFrame(results, columns=results_columns)
print(results)

results.to_csv(f'summary/{version}.csv', index=False)
