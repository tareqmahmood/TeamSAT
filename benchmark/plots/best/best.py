import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import os
import glob


versions = ['cpp_mpi', 'cpp_pthread', 'cpp_omp', 'cpp']
test_names = ['flat30-13', '19x19queens', 'bmc-2', 'bmc-7']

records = []

for version in versions:
    summary_file = f'../../summary/{version}.csv'
    data = pd.read_csv(summary_file)
    data.set_index('test_name', inplace=True)
    for test_name in test_names:
        fastest_ms = round(data.loc[test_name].min(), 3)
        records.append([version, test_name, fastest_ms])

columns = ['variants', 'test_name', 'fastest_ms']
records = pd.DataFrame(records, columns=columns)

# records.to_csv('records.csv', index=False)

sns.set(style="darkgrid")
sns.set(font_scale=1.5)

g = sns.catplot(
    data=records, x="variants", y="fastest_ms", col="test_name",
    kind="bar", ci=None, sharey=False, aspect=0.9
)

g.set_axis_labels("Approaches", "Fastest Execution Time (ms)")
g.set_titles("{col_name}")

# plt.xlabel('Test Cases')
# plt.ylabel('Fastest Execution Time (log ms)')
# plt.legend(title='Variants')
g.set_xticklabels(rotation=30)
plt.savefig('best.pdf', bbox_inches='tight')