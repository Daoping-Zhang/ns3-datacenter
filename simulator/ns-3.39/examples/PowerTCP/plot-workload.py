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

results="./results_workload/"
plots_dir="./plot_workload/"
os.makedirs(plots_dir,exist_ok=True)

# plots_dir="/home/vamsi/Powertcp-NSDI/"
plt.rcParams.update({'font.size': 18,'font.family':'Source Han Sans'})



#algs=list(["powerDelay", "timely","patchedTimely","swift","rttqcn","powerqcn"])
algs=list(["hpcc","timely","swift","ufcc"])

algnames={"dcqcn":"DCQCN","powerInt":"PowerTCP","hpcc":"HPCC","powerDelay":"θ-PowerTCP","timely":"TIMELY","DCTCP":"DCTCP","patchedTimely":"Patched TIMELY","swift":"Swift","rttqcn":"RTT-QCN","powerqcn":"PowerQCN","ufcc":"UFCC"}

#%%

########## WORKLOADS ##################

K=1000
M=K*K
# algs=list(["dcqcn", "powerInt", "hpcc", "powerDelay", "timely", "dctcp"])
# algs=list(["dcqcn", "powerInt", "hpcc", "powerDelay", "timely"])
# algs=list(["powerInt", "hpcc", "powerDelay"])

# algnames={}
# algnames["dcqcn"]="DCQCN"
# algnames["powerInt"]="PowerTCP-INT"
# algnames["hpcc"]="HPCC"
# algnames["powerDelay"]="PowerTCP-Delay"
# algnames["timely"]="TIMELY"
# algnames["DCTCP"]="DCTCP"


markers={}
markers["dcqcn"]="x"
markers["powerInt"]="s"
markers["hpcc"]="^"
markers["powerDelay"]="o"
markers["timely"]="*"
markers["DCTCP"]="."
markers["patchedTimely"]="v"
markers["swift"]=">"
markers["rttqcn"]="<"
markers["ufcc"]="D"


colors={}
colors["dcqcn"]='brown'
colors["powerInt"]='red'
colors["hpcc"]='blue'
colors["powerDelay"]='green'
colors["timely"]='cyan'
colors["DCTCP"]='black'
colors["patchedTimely"]="purple"
colors["swift"]="orange"
colors["rttqcn"]="magenta"
colors["ufcc"]="yellow"




flowStep = [ 0,5*K, 10*K, 20*K, 30*K, 50*K, 75*K, 100*K, 200*K, 400*K,600*K,800*K, 1*M, 5*M, 10*M,30*M  ]
flowSteps= [ 5*K, 10*K, 20*K, 30*K, 50*K, 75*K, 100*K, 200*K, 400*K,600*K,800*K, 1*M, 5*M, 10*M,30*M ]
fS=np.arange(len(flowSteps))
flowSteps= [ "5K", "", "20K","", "50K", "", "100K","", "400K","","800K","", "5M", "","30M" ]
loads=list(["0.2","0.4","0.6","0.8","0.9","0.95"])
REQ_RATE=list(["1","4", "8", "16"])
REQ_SIZE=list(["1000000", "2000000", "4000000", "6000000", "8000000"])



#%%

#######################

# FLOWSIZE VS FCT

# require result-<alg>-<load: 0.2 0.6>-0-0.fct

#######################

req="0"
query="0"
load="0.8"
plt.rcParams.update({'font.size': 18})

figlegend = pylab.figure(figsize=(40.5,1.5))
lenged_elements=list()

for alg in algs:
    lenged_elements.append(Line2D([0],[0], color=colors[alg],marker=markers[alg],lw=10,markersize=30, label=algnames[alg]))

fctsShort={}
for alg in algs:
    fctsShort[alg]={}

for load in ["0.2","0.6"]:
    fig,ax = plt.subplots(1,1)
    ax.set_label("Flow size")
    ax.set_ylabel("99.9 百分位流完成时间减缓倍数")
    ax.set_xlabel("流大小（以字节计）")
    ax.set_yscale('log')
    ax.set_ylim(1,500)
    ax.set_xticks(fS)
    ax.set_xticklabels(flowSteps,rotation=30)
    ax.set_yticks([1,10])
    ax.set_yticklabels([1,10])

    for alg in algs:
        df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct',delimiter=' ',usecols=[1,3,5],names=["fct","size","base"])
        lfct99=list()
        lfct95=list()
        lfct50=list()
        for i in range(1,len(flowStep)):
            df1=df.loc[ (df["size"]<flowStep[i]) & (df["size"] >= flowStep[i-1]) ]
            fcts=df1["fct"].to_list()
            basefcts=df1["base"].to_list()
            sd=list([fcts[i]/basefcts[i] for i in range(len(fcts))])
            sd.sort()
            try:
                fct99 = sd[int(len(sd)*0.999)]
            except:
                fct99 = 0
            try:
                fct95 = sd[int(len(sd)*0.95)]
            except:
                fct95 = 0
            try:
                fct50 = sd[int(len(sd)*0.5)]
            except:
                fct50 = 0
            lfct99.append(fct99)
            lfct95.append(fct95)
            lfct50.append(fct50)
        ax.plot(fS,lfct99,label=algnames[alg],marker=markers[alg],lw=2,markersize=10,c=colors[alg])
        mean_fct = df['fct'].mean()
        mean_base_fat = df['base'].mean()
        #print(f"Average Flow Completion Time for {alg} under load {load}: {mean_fct} base: {mean_base_fat}")
        print(lfct99[8],alg,load)
        fctsShort[alg][load]=lfct99[9]

    fig.tight_layout()
    fig.savefig(plots_dir+'fct-'+load+'.pdf')
    fig.savefig(plots_dir+'fct-'+load+'.png')


figlegend.tight_layout()
figlegend.legend(handles=lenged_elements,loc=9,ncol=5, framealpha=0,fontsize=52)
figlegend.savefig(plots_dir+'fct-legend.pdf')

#%%
# for load in ["0.2","0.6"]:
#     for alg in algs:
#         print ( 100*(fctsShort[alg][load]-fctsShort["powerInt"][load])/fctsShort[alg][load], alg, load)
# print("\n")

# for load in ["0.2","0.6"]:
#     for alg in algs:
#         print ( 100*(fctsShort[alg][load]-fctsShort["powerDelay"][load])/fctsShort[alg][load], alg, load)
#%%

figlegend = pylab.figure(figsize=(40.5,1.5))
lenged_elements=list()

for alg in algs:
    lenged_elements.append(Line2D([0],[0], color=colors[alg],marker=markers[alg],lw=10,markersize=30, label=algnames[alg]))

figlegend.tight_layout()
figlegend.legend(handles=lenged_elements,loc=9,ncol=5, framealpha=0,fontsize=52)
figlegend.savefig(plots_dir+'all-legend.pdf')

#%%

#######################

# average FCT - LONG FLOWS


#######################


# Step 1: 计算 UFCC 算法的平均流完成时间
ufcc_mean_fct_per_interval = None  # 用于存储 UFCC 的平均完成时间
ufcc_alg = 'ufcc'  # UFCC 的算法标识

for alg in algs:
    df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct', delimiter=' ', usecols=[1,3,5], names=["fct", "size", "base"])

    mean_fct_per_interval = list()  # 用于存储每个区间的平均流完成时间
    
    for i in range(1, len(flowStep)):
        df1 = df.loc[(df["size"] < flowStep[i]) & (df["size"] >= flowStep[i-1])]
        
        # 计算每个区间的平均流完成时间
        if not df1.empty:
            mean_fct = df1['fct'].mean()
        else:
            mean_fct = 0  # 如果没有数据，平均值设为0
        mean_fct_per_interval.append(mean_fct)  # 将平均值添加到列表中
        #print(f"UFCC Mean Flow Completion Time: {ufcc_mean_fct_per_interval}")

    # 如果是 UFCC 算法，存储平均完成时间用于后续计算
    if alg == ufcc_alg:
        ufcc_mean_fct_per_interval = mean_fct_per_interval.copy()
        #print(f"UFCC Mean Flow Completion Time: {ufcc_mean_fct_per_interval}")
        break  # 只需要计算一次 UFCC 的数据

# Step 2: 绘制其他算法与 UFCC 的比值
plt.figure(figsize=(8, 6))

for alg in algs:
    if alg == ufcc_alg:
        continue  # UFCC 不用再绘制，直接比较其他算法

    df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct', delimiter=' ', usecols=[1,3,5], names=["fct", "size", "base"])
    
    mean_fct_per_interval = list()  # 用于存储每个区间的平均流完成时间
    
    for i in range(1, len(flowStep)):
        df1 = df.loc[(df["size"] < flowStep[i]) & (df["size"] >= flowStep[i-1])]
        
        # 计算每个区间的平均流完成时间
        if not df1.empty:
            mean_fct = df1['fct'].mean()
        else:
            mean_fct = 0  # 如果没有数据，平均值设为0
        mean_fct_per_interval.append(mean_fct)

    # Step 3: 计算与 UFCC 平均完成时间的比值
    ratio_fct_per_interval = [mean_fct_per_interval[i] / ufcc_mean_fct_per_interval[i] if ufcc_mean_fct_per_interval[i] != 0 else 0 for i in range(len(mean_fct_per_interval))]
    # 绘制比值图
    print(ratio_fct_per_interval)
    plt.plot(flowStep[1:], ratio_fct_per_interval, marker=markers[alg], linestyle='-', color=colors[alg], label=algnames[alg], lw=2, markersize=8)

# Step 4: 设置图表的标题、标签、图例等
plt.title(f'Flow Completion Time Ratio (Algorithm / UFCC) under load {load}')
plt.xlabel('Flow Size (Bytes)')
plt.ylabel('Completion Time Ratio (Relative to UFCC)')
plt.xscale('log')  # 如果流大小跨越多个数量级，可以使用对数坐标
plt.ylim(0.5, 2)  # 固定纵坐标区间为 0 到 50

plt.grid(True)
plt.legend()  # 添加图例

# Step 5: 保存图表
plt.tight_layout()
plt.savefig(plots_dir + f'avg-fct-ratio-to-ufcc-{load}.png')


#######################

# LOAD VS FCT - SHORT FLOWS

# require result-<alg>-<loads: 0.2 0.4 0.6 0.8 0.9 0.95>-0-0.fct

#######################

plt.rcParams.update({'font.size': 20})

# algs=list([ "powerInt", "hpcc", "powerDelay"])

req="0"
query="0"
loads=list(["0.2","0.4","0.6","0.8","0.9","0.95"])

loadInt=list([20,40,60,80,90,95])
fig,ax = plt.subplots(1,1)
ax.xaxis.grid(True,ls='--')
ax.yaxis.grid(True,ls='--')

maxS=5*1*1000
minS=5*1*1000

fctsall={}

for alg in algs:
    fctsall[alg]=list()
    fcts99=list()
    for load in loads:
        df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct',delimiter=' ',usecols=[1,3,5],names=["fct","size","base"])
        df1 = df.loc[(df["size"]<maxS) & (df["size"]> maxS-minS)]
        df1.reset_index(drop=True, inplace=True)
        short=list()
        for i in range(len(df1["fct"])):
            short.append(df1["fct"][i]/df1["base"][i])
        short.sort()
        if len(short) > 0:
            index = min(int(len(short) * 0.999), len(short) - 1)  # 确保索引不超出范围
            fcts99.append(short[index])

    ax.plot(loadInt,fcts99,label=algnames[alg],marker=markers[alg],lw=2,markersize=10,c=colors[alg])
    fctsall[alg]=fcts99

ax.set_xlabel('负载 (%)')
ax.set_ylabel('99.9 百分位流完成时间减缓倍数')
# fig.savefig(plots_dir+'fct'+'.pdf')
# ax.legend(framealpha=0)
fig.tight_layout()
fig.savefig(plots_dir+'fct-shortLoad'+'.pdf')
fig.savefig(plots_dir+'fct-shortLoad'+'.png')

#%%

# for alg in algs:
#     for load in range(len(loads)):
#         print( 100*(fctsall[alg][load]-fctsall["powerInt"][load])/fctsall[alg][load], alg, loads[load] )

#%%


#######################

# LOAD VS FCT - LONG FLOWS

# require result-<alg>-<loads: 0.2 0.4 0.6 0.8 0.9 0.95>-0-0.fct

#######################

# algs=list([ "powerInt", "hpcc", "powerDelay"])

req="0"
query="0"
loads=list(["0.2","0.4","0.6","0.8","0.9","0.95"])

loadInt=list([20,40,60,80,90,95])
fig,ax = plt.subplots(1,1)
ax.xaxis.grid(True,ls='--')
ax.yaxis.grid(True,ls='--')

maxS=10*M
minS=1*M

fctsall={}

for alg in algs:
    fcts99=list()
    fctsall[alg]=list()
    for load in loads:
        df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct',delimiter=' ',usecols=[1,3,5],names=["fct","size","base"])
        df1 = df.loc[(df["size"]<maxS) & (df["size"]> maxS-minS)]
        df1.reset_index(drop=True, inplace=True)
        short=list()
        for i in range(len(df1["fct"])):
            short.append(df1["fct"][i]/df1["base"][i])
        short.sort()
        fcts99.append(short[int(len(short)*0.999)])

    ax.plot(loadInt,fcts99,label=algnames[alg],marker=markers[alg],lw=2,markersize=10,c=colors[alg])
    fctsall[alg]=fcts99

ax.set_xlabel('负载 (%)')
ax.set_ylabel('99.9 百分位流完成时间减缓倍数')
# fig.savefig(plots_dir+'fct'+'.pdf')
# ax.legend(framealpha=0)
fig.tight_layout()
fig.savefig(plots_dir+'fct-longLoad'+'.pdf')
fig.savefig(plots_dir+'fct-longLoad'+'.png')


#%%

# for alg in algs:
#     for load in range(len(loads)):
#         print( 100*(fctsall[alg][load]-fctsall["powerInt"][load])/fctsall[alg][load], alg, loads[load] )


#%%


#######################

# BURSTRATE VS FCT - SHORT FLOWS

# require result-<alg>-0.8-<req: 1 4 8 16>-2M.fct

#######################


req="0"
query="2000000"
loads=list(["0.2","0.4","0.6","0.8","0.9","0.95"])

loadInt=list([20,40,60,80,90,95])
fig,ax = plt.subplots(1,1)
ax.xaxis.grid(True,ls='--')
ax.yaxis.grid(True,ls='--')

maxS=5*1*1000
minS=5*1*1000

fctsall={}

load="0.8"

for alg in algs:
    fcts99=list()
    fctsall[alg]=list()
    for req in REQ_RATE:
        df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct',delimiter=' ',usecols=[1,3,5],names=["fct","size","base"])
        df1 = df.loc[(df["size"]<maxS) & (df["size"]> maxS-minS)]
        df1.reset_index(drop=True, inplace=True)
        short=list()
        for i in range(len(df1["fct"])):
            short.append(df1["fct"][i]/df1["base"][i])
        short.sort()
        fcts99.append(short[int(len(short)*0.999)])

    ax.plot(REQ_RATE,fcts99,label=algnames[alg],marker=markers[alg],lw=2,markersize=10,c=colors[alg])
    fctsall[alg]=fcts99

ax.set_xlabel('请求速率')
ax.set_ylabel('99.9 百分位流完成时间减缓倍数')
fig.tight_layout()
fig.savefig(plots_dir+'fct-shortRate'+'.pdf')
fig.savefig(plots_dir+'fct-shortRate'+'.png')
#%%

# for alg in algs:
#     for req in range(len(REQ_RATE)):
#         print( 100*(fctsall[alg][req]-fctsall["powerInt"][req])/fctsall[alg][req], alg, REQ_RATE[req] )

#%%

#######################

# BURSTRATE VS FCT - LONG FLOWS

# requires result-<alg>-0.8-<req: 1 4 8 16>-2M.fct

#######################


req="0"
query="2000000"
loads=list(["0.2","0.4","0.6","0.8","0.9","0.95"])

loadInt=list([20,40,60,80,90,95])
fig,ax = plt.subplots(1,1)
ax.xaxis.grid(True,ls='--')
ax.yaxis.grid(True,ls='--')

maxS=10*M
minS=1*M

fctsall={}

load="0.8"

for alg in algs:
    fcts99=list()
    avg_fcts = list()  # 平均流完成时间
    fctsall[alg]=list()
    for req in REQ_RATE:
        df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct',delimiter=' ',usecols=[1,3,5],names=["fct","size","base"])
        df1 = df.loc[(df["size"]<maxS) & (df["size"]> maxS-minS)]
        df1.reset_index(drop=True, inplace=True)
        short=list()
        for i in range(len(df1["fct"])):
            short.append(df1["fct"][i]/df1["base"][i])
        short.sort()
        fcts99.append(short[int(len(short)*0.999)])
            # 计算平均流完成时间
        avg_fct = df1["fct"].mean() / df1["base"].mean()
        avg_fcts.append(avg_fct)

    #ax.plot(REQ_RATE,fcts99,label=algnames[alg],marker=markers[alg],lw=2,markersize=10,c=colors[alg])
    # 绘制平均流完成时间曲线
    ax.plot(REQ_RATE, avg_fcts, label=algnames[alg] + " Avg", marker=markers[alg], lw=2, markersize=10, linestyle='--', c=colors[alg])

    fctsall[alg]=fcts99

ax.set_xlabel('请求速率')
ax.set_ylabel('99.9 百分位流完成时间减缓倍数')
fig.tight_layout()
fig.savefig(plots_dir+'fct-longRate'+'.pdf')
fig.savefig(plots_dir+'fct-longRate'+'.png')
#%%

# for alg in algs:
#     for req in range(len(REQ_RATE)):
#         print( 100*(fctsall[alg][req]-fctsall["powerInt"][req])/fctsall[alg][req], alg, REQ_RATE[req] )

#%%

#######################

# BURSTSIZE VS FCT - SHORT FLOWS

# require result-<alg>-0.8-4-<reqsize: 1M 2M 4M 8M>.fct

#######################


req="4"
query="0"
loads=list(["0.2","0.4","0.6","0.8","0.9","0.95"])

loadInt=list([20,40,60,80,90,95])
fig,ax = plt.subplots(1,1)
ax.xaxis.grid(True,ls='--')
ax.yaxis.grid(True,ls='--')

ax.set_xticks(np.arange(len(REQ_SIZE)))
ax.set_xticklabels([1,2,4,6,8])

maxS=5*1*1000
minS=5*1*1000

fctsall={}

load="0.8"

for alg in algs:
    fcts99=list()
    fctsall[alg]=list()
    for query in REQ_SIZE:
        df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct',delimiter=' ',usecols=[1,3,5],names=["fct","size","base"])
        df1 = df.loc[(df["size"]<maxS) & (df["size"]> maxS-minS)]
        df1.reset_index(drop=True, inplace=True)
        short=list()
        for i in range(len(df1["fct"])):
            short.append(df1["fct"][i]/df1["base"][i])
        short.sort()
        fcts99.append(short[int(len(short)*0.999)])

    ax.plot(np.arange(len(REQ_SIZE)),fcts99,label=algnames[alg],marker=markers[alg],lw=2,markersize=10,c=colors[alg])
    fctsall[alg]=fcts99

ax.set_xlabel('请求大小 (MB)')
ax.set_ylabel('99.9 百分位流完成时间减缓倍数')
fig.tight_layout()
fig.savefig(plots_dir+'fct-shortBurst4'+'.pdf')
fig.savefig(plots_dir+'fct-shortBurst4'+'.png')
#%%

# for alg in algs:
#     for req in range(len(REQ_SIZE)):
#         print( 100*(fctsall[alg][req]-fctsall["powerDelay"][req])/fctsall[alg][req], alg, REQ_SIZE[req] )

#%%

#######################

# BURSTSIZE VS FCT - LONG FLOWS

# require result-<alg>-0.8-4-<reqsize: 1M 2M 4M 8M>.fct

#######################


req="4"
query="2000000"
loads=list(["0.2","0.4","0.6","0.8","0.9","0.95"])

loadInt=list([20,40,60,80,90,95])
fig,ax = plt.subplots(1,1)
ax.xaxis.grid(True,ls='--')
ax.yaxis.grid(True,ls='--')
ax.set_xticks(np.arange(len(REQ_SIZE)))
ax.set_xticklabels([1,2,4,6,8])

maxS=10*M
minS=1*M

fctsall={}

load="0.8"

for alg in algs:
    fcts99=list()
    fctsall[alg]=list()
    avg_fcts = list()  # 平均流完成时间

    for query in REQ_SIZE:
        df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.fct',delimiter=' ',usecols=[1,3,5],names=["fct","size","base"])
        df1 = df.loc[(df["size"]<maxS) & (df["size"]> maxS-minS)]
        df1.reset_index(drop=True, inplace=True)
        short=list()
        for i in range(len(df1["fct"])):
            short.append(df1["fct"][i]/df1["base"][i])
        short.sort()
        fcts99.append(short[int(len(short)*0.999)])
                # 计算平均流完成时间
        avg_fct = df1["fct"].mean() / df1["base"].mean()
        avg_fcts.append(avg_fct)

    # 绘制平均流完成时间曲线
    #ax.plot(REQ_RATE, avg_fcts, label=algnames[alg] + " Avg", marker=markers[alg], lw=2, markersize=10, linestyle='--', c=colors[alg])

    ax.plot(np.arange(len(REQ_SIZE)),avg_fcts,label=algnames[alg],marker=markers[alg],lw=2,markersize=10,c=colors[alg])
    fctsall[alg]=fcts99

ax.set_xlabel('请求大小 (MB)')
ax.set_ylabel('99.9 百分位流完成时间减缓倍数')
fig.tight_layout()
fig.savefig(plots_dir+'fct-longBurst4'+'.pdf')
fig.savefig(plots_dir+'fct-longBurst4'+'.png')


#%%

# for alg in algs:
#     for req in range(len(REQ_SIZE)):
#         print( 100*(fctsall[alg][req]-fctsall["powerDelay"][req])/fctsall[alg][req], alg, REQ_SIZE[req] )

#%%

#######################

# CDF BUFFER 0.8 LOAD

# require result-<alg>-0.8-0-0.buf

#######################

alg="powerInt"
load="0.8" # don't know why it is 0.9 in the original code
query="2000000"
req="8"

query="0"
req="0"
plt.rcParams.update({'font.size': 20})

fig,ax=plt.subplots(1,1)
ax.set_xticks([0,50*K,100*K,150*K,200*K])
ax.set_xticklabels(["0","50","100","150","200"])
ax.set_ylabel("CDF")
ax.set_xlabel("缓冲区占用 (KB)")

for alg in algs:
    df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.buf',delimiter=' ',usecols=[3],names=["qlen"])

    qlen=df["qlen"].tolist()
    sortQlen= np.sort(qlen)
    p = 1. * np.arange(len(sortQlen))/(len(sortQlen) - 1)


    ax.plot(sortQlen,p,c=colors[alg],lw=2)
    ax.plot(sortQlen[len(sortQlen)-1],1,marker=markers[alg],markersize=10,c=colors[alg],label=algnames[alg])

fig.tight_layout()
fig.savefig(plots_dir+'buf-load'+'.pdf')
fig.savefig(plots_dir+'buf-load'+'.png')
#%%


#######################

# CDF BUFFER 2MB BURST, 16 Rate

# require result-<alg>-0.8-16-2M.buf

#######################

alg="powerInt"
load="0.8"
query="2000000"
req="16"

plt.rcParams.update({'font.size': 20})

fig,ax=plt.subplots(1,1)
ax.set_xticks([0,0.5*M,1*M,1.5*M,2*M,2.5*M])
ax.set_xticklabels(["0","0.5","1","1.5","2","2.5"])
ax.set_ylabel("CDF")
ax.set_xlim(-50*K,1.5*M)
ax.set_xlabel("缓冲区占用 (MB)")

cdfs=pd.DataFrame()

for alg in algs:
    df = pd.read_csv(results+'result-'+alg+'-'+load+'-'+req+'-'+query+'.buf',delimiter=' ',usecols=[3],names=["qlen"])
    qlen=df["qlen"].tolist()
    sortQlen= np.sort(qlen)
    p = 1. * np.arange(len(sortQlen))/(len(sortQlen) - 1)
    ax.plot(sortQlen,p,c=colors[alg],lw=2)
    ax.plot(sortQlen[len(sortQlen)-1],1,marker=markers[alg],markersize=10,c=colors[alg],label=algnames[alg])
    cdfs[alg]=sortQlen
    cdfs["cdf"]=p
fig.tight_layout()
fig.savefig(plots_dir+'buf-burst-16'+'.pdf')
fig.savefig(plots_dir+'buf-burst-16'+'.png')
#%%


