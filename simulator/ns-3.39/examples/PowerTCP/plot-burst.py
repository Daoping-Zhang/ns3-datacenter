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

results="./results_burst/"
plots_dir="./plot_burst/"
os.makedirs(plots_dir,exist_ok=True)

# plots_dir="/home/vamsi/Powertcp-NSDI/"
plt.rcParams.update({'font.size': 18,'font.family':'PingFang SC'})


algs=list(["dcqcn", "powerInt", "hpcc", "powerDelay", "timely", "dctcp","patchedTimely","swift","rttqcn"])
algnames={"dcqcn":"DCQCN","powerInt":"PowerTCP","hpcc":"HPCC","powerDelay":r'$\theta-PowerTCP$',"timely":"TIMELY","DCTCP":"DCTCP","patchedTimely":"Patched TIMELY","swift":"Swift","rttqcn":"RTT-QCN"}


#%%

######### BURST ###############

plt.rcParams.update({'font.size': 22})


figlegend = pylab.figure(figsize=(11.5,1.5))
lenged_elements=list()

# red green blue brownm grey
colorsBurst=list(["#1979a9","red", "#478fb5","tab:brown","tab:gray"])
labels=list(['吞吐量','队列长度'])

for i in range(1,3):
    lenged_elements.append(Line2D([0],[0], color=colorsBurst[i-1],lw=6, label=labels[i-1]))

for alg in algs:

    df = pd.read_csv(results+'result-'+alg+'.burst',delimiter=' ',usecols=[5,9,11,13],names=["th","qlen","time","power"])

    fig,ax = plt.subplots(1,1)
    # fig.suptitle(alg)
    ax.xaxis.grid(True,ls='--')
    ax.yaxis.grid(True,ls='--')
    ax1=ax.twinx()
    ax.set_yticks([10e9,25e9,40e9,80e9,100e9])
    ax.set_yticklabels(["10","25","40","80","100"])
    ax.set_ylabel("吞吐量 (Gbps)")

    start=0.15
    xtics=[i*0.001+start for i in range(0,6)]
    ax.set_xticks(xtics)
    xticklabels=[str(i) for i in range(0,6)]
    ax.set_xticklabels(xticklabels)

    ax.set_xlabel("时间 (ms)")
    ax.set_xlim(0.1495,0.154)
    ax.plot(df["time"],df["th"],label="吞吐量",c='#1979a9',lw=2)
    ax1.set_ylim(0,600)
    ax1.set_ylabel("队列长度 (KB)")
    ax1.plot(df["time"],df["qlen"]/(1000),c='r',label="队列长度",lw=2)
    # ax.legend(loc=1)
    # ax1.legend(loc=3)
    # fig.legend(loc=2,ncol=2,framealpha=0,borderpad=-0.1)
    fig.tight_layout()
    fig.savefig(plots_dir+alg+'.pdf')
    fig.savefig(plots_dir+alg+'.png')

    fig1,ax2 = plt.subplots(1,1)
    # fig.suptitle(alg)
    ax2.xaxis.grid(True,ls='--')
    ax2.yaxis.grid(True,ls='--')
    ax3=ax2.twinx()
    ax2.set_yticks([10e9,25e9,40e9,80e9,100e9])
    ax2.set_yticklabels(["10","25","40","80","100"])
    ax2.set_ylabel("吞吐量 (Gbps)")

    start=0.15
    xtics=[i*0.001+start for i in range(0,6)]
    ax2.set_xticks(xtics)
    xticklabels=[str(i) for i in range(0,6)]
    ax2.set_xticklabels(xticklabels)
    ax2.set_xlabel("时间 (ms)")
    ax2.set_xlim(0.1495,0.154)
    ax2.plot(df["time"],df["th"],label="吞吐量",c='#1979a9',lw=2)
    ax3.set_ylabel("归一化功率")
    ax3.set_ylim(0,2)
    ax3.plot(df["time"],df["power"],c='g',label="归一化功率",lw=2)
    fig1.tight_layout()
    fig1.savefig(plots_dir+alg+'-power.pdf')
    fig1.savefig(plots_dir+alg+'-power.png')


figlegend.tight_layout()
figlegend.legend(handles=lenged_elements,loc=9,ncol=2, framealpha=0,fontsize=48)
# figlegend.savefig(plots_dir+'/burst/burst-legend.pdf')
