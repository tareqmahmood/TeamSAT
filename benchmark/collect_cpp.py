import pandas as pd 
import glob

version = 'cpp'

test_cases = pd.read_csv('test_cases.csv')

results = []
results_columns = ['test_name', 'mean_ms']

for i, row in test_cases.iterrows():
    result_file = f'results/{version}/{row.test_name}.txt'
    times = []
    with open(result_file) as file:
        for line in file:
            line = line.strip()
            line = line.split()
            if line[0] == 'Time' and len(line) == 2:
                times.append(float(line[1]))
    if len(times) > 0:
        mean_ms = round(sum(times) / len(times), 3)
        results.append((row.test_name, mean_ms))

results = pd.DataFrame(results, columns=results_columns)
print(results)

results.to_csv(f'summary/{version}.csv', index=False)
