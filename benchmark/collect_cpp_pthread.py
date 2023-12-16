import pandas as pd 
import glob

version = 'cpp_pthread'

n_threads = [2, 4, 8, 12, 16]

results = []
results_columns = ['test_name']
for t in n_threads:
    results_columns.append(f'mean_ms_{t}_threads')

test_cases = pd.read_csv('test_cases.csv')


for i, row in test_cases.iterrows():
    record = [row.test_name]
    for t in n_threads:
        result_file = f'results/{version}/{t}/{row.test_name}.txt'
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
