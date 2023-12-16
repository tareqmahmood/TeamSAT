import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import os
import glob


version = 'cpp_pthread'
test_names = ['flat30-13', '19x19queens', 'bmc-2', 'bmc-7']

records = []

summary_file = f'../../summary/{version}.csv'
data = pd.read_csv(summary_file)
data.set_index('test_name', inplace=True)

records = []

for test_name in test_names:
    for column in data.columns:
        ms = data.loc[test_name][column]
        n_threads = int(column.split('_')[2])
        records.append((test_name, n_threads, ms))
    

columns = ['test_name', 'n_threads', 'ms']
records = pd.DataFrame(records, columns=columns)

print(records)

sns.set(style="darkgrid")

# g = sns.catplot(
#     data=records, x="n_threads", y="ms", col="test_name",
#     kind="bar", ci=None, sharey=False, aspect=0.7
# )

g = sns.relplot(
    data=records, x="n_threads", y="ms", col="test_name",
    kind="line", aspect=0.7, facet_kws={'sharey': False, 'sharex': True},
    markers=True, style='test_name', hue='test_name', linewidth=2
)

g.set_axis_labels("# Threads", "Execution Time (ms)")
g.set_titles("{col_name}")
g._legend.remove()

plt.xticks([2, 4, 6, 8, 12, 16])
plt.savefig(f'{version}_trend.pdf', bbox_inches='tight')