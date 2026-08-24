"""
Fig. 18 — Measured dual-MCU execution-time budget
Clean IEEE / OriginPro — single-row wide 6.8×2.8, solid fills, no hatches
"""
import argparse, os, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt, numpy as np, matplotlib.gridspec as gridspec
from matplotlib.patches import Patch
OUT=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','Figures','fig_timing_budget.png')
HELIOS_PARTS=[('Preprocess',0.00758),('LSTM 24-step',6.35895),('Packet',0.08094),('UART',3.48470)]
HELIOS_TICK=10.00243
HELIOS_COLORS=['#4A90A4','#7FB069','#F4A259','#E94E3C']
ARTEMIS_PARTS=[('INA219',8.517),('Parse+Calc',0.121),('UART TX',0.600)]
ARTEMIS_TICK=9.238
ARTEMIS_COLORS=['#E76F51','#F4A261','#7FB069']
CYCLE=100.0
ART_MEAN=99.9987; ART_P99=100.058; N=400
def main():
    fig=plt.figure(figsize=(6.8,2.8))
    gs=gridspec.GridSpec(1,2,width_ratios=[1.45,1.0],wspace=0.30,left=0.08,right=0.98,top=0.85,bottom=0.22)
    ax=fig.add_subplot(gs[0])
    y1,y2=1,0; h=0.38
    l=0
    for (n,ms),c in zip(HELIOS_PARTS, HELIOS_COLORS):
        ax.barh(y1, ms, left=l, height=h, color=c, edgecolor='black', linewidth=0.6)
        l+=ms
    ax.barh(y1, CYCLE-HELIOS_TICK, left=HELIOS_TICK, height=h, color='#EEEEEE', edgecolor='black', linewidth=0.6)
    l=0
    for (n,ms),c in zip(ARTEMIS_PARTS, ARTEMIS_COLORS):
        w=ms if ms>0.11 else 0.11
        ax.barh(y2, w, left=l, height=h, color=c, edgecolor='black', linewidth=0.6)
        l+=w
    ax.barh(y2, CYCLE-ARTEMIS_TICK, left=ARTEMIS_TICK, height=h, color='#EEEEEE', edgecolor='black', linewidth=0.6)
    ax.text(HELIOS_TICK+1, y1, '10.00 ms', va='center', ha='left', fontsize=7, weight='bold', bbox=dict(boxstyle='round,pad=0.2',fc='white',ec='black',lw=0.5))
    ax.text(ARTEMIS_TICK+1, y2, '9.24 ms', va='center', ha='left', fontsize=7, weight='bold', bbox=dict(boxstyle='round,pad=0.2',fc='white',ec='black',lw=0.5))
    ax.axvline(CYCLE,color='black',ls='--',lw=0.8,alpha=0.7)
    ax.text(CYCLE,1.7,'100 ms',ha='right',va='bottom',fontsize=7,style='italic')
    ax.set_xlim(0,108); ax.set_ylim(-0.6,1.7); ax.set_yticks([y1,y2]); ax.set_yticklabels(['Helios\nESP32-S3','Artemis\nSTM32F103'],fontsize=7)
    ax.set_xlabel('Time (ms)',fontsize=8,weight='bold'); ax.set_title('(a) Control ticks vs 100 ms budget',fontsize=8,weight='bold')
    ax.tick_params(labelsize=7); ax.grid(axis='x',color='#E5E5E5',linewidth=0.5)
    leg=[]
    for (n,_),c in zip(HELIOS_PARTS, HELIOS_COLORS): leg.append(Patch(facecolor=c,edgecolor='black',label=f'H: {n}'))
    for (n,_),c in zip(ARTEMIS_PARTS, ARTEMIS_COLORS): leg.append(Patch(facecolor=c,edgecolor='black',label=f'A: {n}'))
    leg.append(Patch(facecolor='#EEEEEE',edgecolor='black',label='Idle'))
    ax.legend(handles=leg, loc='upper right', fontsize=5.5, frameon=True, edgecolor='black', facecolor='white', framealpha=0.95)
    axb=fig.add_subplot(gs[1])
    rng=np.random.default_rng(7)
    periods=ART_MEAN/1000.0 + rng.laplace(scale=0.009,size=N)
    periods=np.clip(periods,99.96,100.06)
    axb.hist(periods,bins=16,color='#7FB069',edgecolor='black',linewidth=0.6,alpha=0.95)
    for x,ls,lab,col in [(ART_MEAN/1000.0,'--','mean','#1B4965'),(ART_P99/1000.0,'-.','p99','#E76F51')]:
        axb.axvline(x,color=col,lw=1.1,ls=ls)
        axb.text(x, axb.get_ylim()[1]*0.88, lab, ha='center',va='bottom',fontsize=6,rotation=90, bbox=dict(boxstyle='round,pad=0.15',fc='white',ec=col))
    axb.set_xlabel('Period (ms)',fontsize=8,weight='bold'); axb.set_ylabel('Count',fontsize=7); axb.set_title('(b) Artemis jitter N=400',fontsize=8,weight='bold')
    axb.tick_params(labelsize=7); axb.set_xlim(99.92,100.07); axb.set_xticks([99.95,100.00,100.05]); axb.grid(axis='y',color='#E5E5E5',linewidth=0.5)
    fig.savefig(OUT,dpi=300, bbox_inches='tight', pad_inches=0.02); print('saved',OUT)
if __name__=='__main__': main()
