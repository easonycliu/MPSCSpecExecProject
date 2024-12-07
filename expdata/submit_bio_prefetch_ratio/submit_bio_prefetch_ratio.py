#!/usr/bin/env python3

import os

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

if __name__ == "__main__":
	data_src = "submit_bio_prefetch_ratio"
	submit_bio_prefetch_ratio_df = pd.read_csv(os.path.join(os.getcwd(), "expdata", "submit_bio_prefetch_ratio", "{}.csv".format(data_src)), header=None)
	submit_io_time = np.array(submit_bio_prefetch_ratio_df.iloc[1].values.tolist())
	prefetch_time = np.array(submit_bio_prefetch_ratio_df.iloc[2].values.tolist())
	plt.bar(np.log2(submit_bio_prefetch_ratio_df.iloc[0].values.tolist()), prefetch_time, label="prefetch time")
	plt.bar(np.log2(submit_bio_prefetch_ratio_df.iloc[0].values.tolist()), submit_io_time, label="submit_bio time")
	plt.xlabel("Log2 of Prefetch Batching Segment Number")
	plt.ylabel("Blocking Time (us)")
	plt.legend(loc="upper left")
	plt.twinx()
	plt.plot(np.log2(submit_bio_prefetch_ratio_df.iloc[0].values.tolist()), submit_io_time / prefetch_time, label="submit_bio ratio", marker='o')
	plt.ylabel("submit_bio time / prefetch time")
	plt.legend(loc="center left")
	plt.savefig(os.path.join(os.getcwd(), "expdata", "submit_bio_prefetch_ratio", "{}.png".format(data_src)))
