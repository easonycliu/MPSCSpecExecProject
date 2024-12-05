#!/usr/bin/env python3

import os

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

if __name__ == "__main__":
	data_src = "prefetch_blocking_time_wo_workingset"
	prefetch_blocking_time_df = pd.read_csv(os.path.join(os.getcwd(), "expdata", "prefetch_blocking_time", "{}.csv".format(data_src)), header=None)
	plt.plot(np.log2(prefetch_blocking_time_df.iloc[0].values.tolist()), prefetch_blocking_time_df.iloc[1].values.tolist(), label="Prefetch Blocking Avg Time", marker='o')
	plt.plot(np.log2(prefetch_blocking_time_df.iloc[0].values.tolist()), prefetch_blocking_time_df.iloc[2].values.tolist(), label="Prefetch Blocking Total Time", marker='o')
	plt.xlabel("Log2 of Prefetch Batching Segment Number")
	plt.ylabel("Blocking Time (us)")
	plt.legend()
	plt.savefig(os.path.join(os.getcwd(), "expdata", "prefetch_blocking_time", "{}.png".format(data_src)))
