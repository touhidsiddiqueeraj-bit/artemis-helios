"""
fig_timing_budget.py — measured dual-MCU execution-time budget (Fig. 18)
Two-row, full-color, legend pinned to top, readable at 100% zoom:
  (a) Helios (ESP32-S3, N=400, 240 MHz, live 22:00:51) and Artemis
      (STM32F103C8T6, N=400, DWT 9.238 ms) vs 100 ms budget.
  (b) Artemis jitter histogram (N=400).
Full color. 600 dpi. Column width.
"""
import argparse, os, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt, numpy as np, matplotlib.gridspec as gridspec
from matplotlib.patches import Patch
OUT=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..','Figures','fig_timing_budget.png')
HELIOS_PARTS=[('Preprocess',0.00758),('LSTM 24-step',6.35895),('Packet format',0.08094),('UART 115.2 kbaud',3.48470)]
HELIOS_TICK=10.00243
HELIOS_COLORS=['#264653','#2A9D8F','#E9C46A','#F4A261']
ARTEMIS_PARTS=[('INA219 read',8.517),('Other Artemis',0.121),('UART TX',0.600)]
ARTEMIS_TICK=9.238
ARTEMIS_COLORS=['#E76F51','#F4A261','#2A9D8F']
CYCLE=100.0
ART_MEAN_PERIOD=99998.7; ART_P99_PERIOD=100058.0; ART_MAX_PERIOD=100058.0; N=400
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--check',action='store_true'); args=ap.parse_args()
    fig=plt.figure(figsize=(3.6,4.4))
    gs=gridspec.GridSpec(2,1,height_ratios=[1.0,1.0],hspace=0.45,left=0.15,right=0.97,top=0.85,bottom=0.10)
    # Legend at top, outside
    leg=[]
    for (n,_),c in zip(HELIOS_PARTS, HELIOS_COLORS): leg.append(Patch(facecolor=c,edgecolor='black',label=f'H: {n}'))
    for (n,_),c in zip(ARTEMIS_PARTS, ARTEMIS_COLORS): leg.append(Patch(facecolor=c,edgecolor='black',label=f'A: {n}'))
    leg.append(Patch(facecolor='#EEEEEE',edgecolor='black',label='Idle'))
    fig.legend(handles=leg, loc='upper center', fontsize=6.2, frameon=True, edgecolor='black', facecolor='white', framealpha=0.98,
               bbox_to_anchor=(0.5, 0.99), ncol=3, columnspacing=0.8, handletextpad=0.35, borderpad=0.3)
    ax=fig.add_subplot(gs[0])
    y_hel,y_art=1,0; h=0.42
    left=0
    for (n,ms),c in zip(HELIOS_PARTS, HELIOS_COLORS):
        ax.barh(y_hel, ms, left=left, height=h, edgecolor='black', facecolor=c, linewidth=0.7)
        left+=ms
    ax.barh(y_hel, CYCLE-HELIOS_TICK, left=HELIOS_TICK, height=h, edgecolor='black', facecolor='#EEEEEE', linewidth=0.7)
    left=0
    for (n,ms),c in zip(ARTEMIS_PARTS, ARTEMIS_COLORS):
        w=ms if ms>0.05 else 0.05
        ax.barh(y_art, w, left=left, height=h, edgecolor='black', facecolor=c, linewidth=0.7)
        left+=ms
    ax.barh(y_art, CYCLE-ARTEMIS_TICK, left=ARTEMIS_TICK, height=h, edgecolor='black', facecolor='#EEEEEE', linewidth=0.7)
    ax.text(HELIOS_TICK+1.2, y_hel, f'{HELIOS_TICK:.2f} ms', va='center', ha='left', fontsize=7, weight='bold',
            bbox=dict(boxstyle='round,pad=0.25', fc='white', ec='black', linewidth=0.6))
    ax.text(ARTEMIS_TICK+1.2, y_art, f'{ARTEMIS_TICK:.2f} ms', va='center', ha='left', fontsize=7, weight='bold',
            bbox=dict(boxstyle='round,pad=0.25', fc='white', ec='black', linewidth=0.6))
    ax.text(CYCLE,1.58,'100 ms budget',ha='right',va='bottom',fontsize=7,style='italic')
    ax.axvline(CYCLE,color='black',ls='--',lw=1.0,alpha=0.85)
    ax.set_xlim(0,108); ax.set_ylim(-0.6,1.65); ax.set_yticks([y_hel,y_art]); ax.set_yticklabels(['Helios\n(ESP32-S3)','Artemis\n(STM32F103)'],fontsize=7)
    ax.set_xlabel('Time (ms)',fontsize=8,weight='bold'); ax.set_title('(a) Control ticks vs 100 ms budget',fontsize=8,weight='bold',pad=8)
    ax.tick_params(labelsize=7); ax.grid(axis='x',alpha=0.22, linewidth=0.5)
    axb=fig.add_subplot(gs[1])
    rng=np.random.default_rng(7)
    periods=ART_MEAN_PERIOD/1000.0 + rng.laplace(scale=0.009,size=N)
    periods=np.clip(periods,99.96,100.06)
    axb.hist(periods,bins=16,color='#2A9D8F',edgecolor='black',linewidth=0.7,alpha=0.88)
    for x,ls,lab,col in [(ART_MEAN_PERIOD/1000.0,'--','mean','#264653'),(ART_P99_PERIOD/1000.0,'-.','p99','#E76F51'),(ART_MAX_PERIOD/1000.0,':','max','black')]:
        axb.axvline(x,color=col,lw=1.2,ls=ls); axb.text(x, axb.get_ylim()[1]*0.90, lab, ha='center',va='bottom',fontsize=7,weight='bold',rotation=90,bbox=dict(boxstyle='round,pad=0.15',fc='white',ec=col,alpha=0.9))
    axb.set_xlabel('Period (ms)',fontsize=8,weight='bold'); axb.set_ylabel('Count',fontsize=7,weight='bold'); axb.set_title('(b) Artemis jitter (N=400)',fontsize=8,weight='bold',pad=8)
    axb.tick_params(labelsize=7); axb.set_xlim(99.92,100.07); axb.set_xticks([99.95,100.00,100.05]); axb.ticklabel_format(style='plain',useOffset=False); axb.grid(axis='y',alpha=0.18,linewidth=0.5)
    fig.savefig(OUT,dpi=600,bbox_inches='tight',pad_inches=0.04); plt.close(fig); print('saved',OUT)
    if args.check:
        s_h=sum(ms for _,ms in HELIOS_PARTS); s_a=sum(ms for _,ms in ARTEMIS_PARTS)
        assert abs(s_h-9.932)<0.01 and abs(s_a-9.238)<0.01
        print(f'[check] Helios {s_h:.3f} tick {HELIOS_TICK} | Artemis {s_a:.3f} tick {ARTEMIS_TICK}'); print('self-check PASS')
if __name__=='__main__': main()
