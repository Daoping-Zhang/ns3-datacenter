#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Mar  1 06:46:30 2021

@author: vamsi
"""
import os
import requests
import pandas as pd
import numpy as np
import math
import matplotlib.pyplot as plt
import pylab
from matplotlib.lines import Line2D
import multiprocessing


results = "./results_fairness/"
plots_dir = "./plot_fairness/"
os.makedirs(plots_dir, exist_ok=True)

# plots_dir="/home/vamsi/Powertcp-NSDI/"
plt.rcParams.update({'font.size': 18, 'font.family': 'Source Han Sans'})


#algs = list(["powerDelay", "timely", "patchedTimely",
 #           "swift", "rttqcn", "powerqcn"])
algs=list(["ufcc"])

algnames = {"dcqcn": "DCQCN", "powerInt": "PowerTCP", "hpcc": "HPCC", "powerDelay": "θ-PowerTCP", "timely": "TIMELY",
            "DCTCP": "DCTCP", "patchedTimely": "Patched TIMELY", "swift": "Swift", "rttqcn": "RTT-QCN", "powerqcn": "PowerQCN"}


######## FAIRNESS #############

# results=NS3+"examples/PowerTCP/results_fairness/"

plt.rcParams.update({'font.size': 24})

figlegend = pylab.figure(figsize=(20.5, 1.5))
lenged_elements = list()

colorsFair = list(["#d65151", "#7ab547", "#478fb5", "tab:brown", "tab:gray"])
labels = list(['flow-1', 'flow-2', 'flow-3', 'flow-4', 'flow-5'])

for i in range(1, 5):
    lenged_elements.append(
        Line2D([0], [0], color=colorsFair[i-1], lw=6, label=labels[i-1]))


for alg in algs:

    # if (alg=="powerDelay"):
    # continue

    fig, ax = plt.subplots(1, 1)
    ax.xaxis.grid(True, ls='--')
    ax.yaxis.grid(True, ls='--')

    ax.set_ylabel("吞吐量 (Gbps)")
    ax.set_xlabel("时间 (s)")
    # fig.suptitle(alg)

    df1 = pd.read_csv(results+'result-'+alg+'.1', delimiter=' ',
                      usecols=[5, 7], names=["th", "time"])
    df2 = pd.read_csv(results+'result-'+alg+'.2', delimiter=' ',
                      usecols=[5, 7], names=["th", "time"])
    df3 = pd.read_csv(results+'result-'+alg+'.3', delimiter=' ',
                      usecols=[5, 7], names=["th", "time"])
    df4 = pd.read_csv(results+'result-'+alg+'.4', delimiter=' ',
                      usecols=[5, 7], names=["th", "time"])

    ax.set_xlim(0, 0.7)

    # ax.plot(df1["time"][::100],df1["th"][::100]/1e9)
    # ax.plot(df2["time"][::100],df2["th"][::100]/1e9)
    # ax.plot(df3["time"][::100],df3["th"][::100]/1e9)
    # ax.plot(df4["time"][::100],df4["th"][::100]/1e9)

    ax.plot(df1["time"], df1["th"]/1e9, c=colorsFair[0])
    ax.plot(df2["time"], df2["th"]/1e9, c=colorsFair[1])
    ax.plot(df3["time"], df3["th"]/1e9, c=colorsFair[2])
    ax.plot(df4["time"], df4["th"]/1e9, c=colorsFair[3])

    fig.tight_layout()
    fig.savefig(plots_dir+alg+'.pdf')
    fig.savefig(plots_dir+alg+'.png')
    print("Saved fairness throughput plot for", alg)

figlegend.tight_layout()
figlegend.legend(handles=lenged_elements, loc=9,
                 ncol=5, framealpha=0, fontsize=48)
# figlegend.savefig(plots_dir+'/fairness/fair-legend.pdf')

###### Jain's Fairness Index ########


def calculate_jains_index(throughputs):
    throughputs = np.array(throughputs)
    throughputs = throughputs[throughputs > 0]
    if throughputs.size == 0:
        return 0
    sum_x = throughputs.sum()
    sum_x_squared = np.square(throughputs).sum()
    n = throughputs.size
    return (sum_x ** 2) / (n * sum_x_squared)


def process_algorithm(alg):
    dfs = [
        pd.read_csv(f"{results}result-{alg}.1", delimiter=' ',
                    usecols=[5, 7], names=["th", "time"]),
        pd.read_csv(f"{results}result-{alg}.2", delimiter=' ',
                    usecols=[5, 7], names=["th", "time"]),
        pd.read_csv(f"{results}result-{alg}.3", delimiter=' ',
                    usecols=[5, 7], names=["th", "time"]),
        pd.read_csv(f"{results}result-{alg}.4", delimiter=' ',
                    usecols=[5, 7], names=["th", "time"])
    ]

    times = dfs[0]["time"]
    fairness_indices = [calculate_jains_index(
        [df.loc[df['time'] == t, 'th'].iloc[0] for df in dfs]) for t in times]

    # Save or return results as needed
    return alg, fairness_indices, times


with multiprocessing.Pool(processes=len(algs)) as pool:
    results = pool.map(process_algorithm, algs)
for result in results:
    alg, fairness_indices, times = result
    avg = sum(fairness_indices) / len([x for x in fairness_indices if x > 0])

    fig, ax2 = plt.subplots(1, 1)
    ax2.xaxis.grid(True, ls='--')
    ax2.yaxis.grid(True, ls='--')
    ax2.set_ylim(0, 1)

    ax2.set_ylabel("公平性指数")
    ax2.set_xlabel("时间 (s)")

    ax2.set_xlim(0, 0.7)
    ax2.plot(times, fairness_indices, c='blue')

    fig.tight_layout()
    fig.savefig(plots_dir+alg+'_jain.pdf')
    fig.savefig(plots_dir+alg+'_jain.png')
    print("Saved Jain's fairness index plot for", alg, "with average", avg)
