"""
Fig 9: Field validation - simulated vs IDCOL SHS baseline
"""
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'output')
os.makedirs(OUT, exist_ok=True)

plt.rcParams.update({
    'font.family':'serif','font.serif':['Times New Roman','DejaVu Serif'],
    'font.size':8,'axes.labelsize':8,'axes.titlesize':8.5,
    'xtick.labelsize':7,'ytick.labelsize':7,
    'legend.fontsize':7,'legend.framealpha':0.9,
    'lines.linewidth':1.0,'axes.linewidth':0.6,
    'grid.linewidth':0.35,'grid.alpha':0.35,
    'figure.dpi':300,'savefig.dpi':300,
    'savefig.bbox':'tight','savefig.pad_inches':0.04,
})

C1,C2 = 3.5, 7.16
B,R,G,O,P,GR = '#1565C0','#C62828','#2E7D32','#E65100','#6A1B9A','#546E7A'

def fig9():
    fig,(a1,a2)=plt.subplots(1,2,figsize=(C2,2.9))
    mn=np.arange(1,13); mn_n='Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec'.split()
    hoss=[82,83,85,86,84,80,79,81,83,85,84,82]
    ours=[89,90,92,93,91,88,94,91,92,93,91,90]
    po  =[75,76,79,80,78,71,71,73,76,78,77,76]
    a1.plot(mn,hoss,'s-',color=R,ms=5,lw=0.85,label='Hossain et al. (field, IDCOL SHS)')
    a1.plot(mn,ours,'o-',color=B,ms=4,lw=0.85,label='Helios-Artemis (simulation)')
    a1.plot(mn,po,'^--',color=GR,ms=3,lw=0.7,label='Plain P&O (simulation)')
    a1.fill_between(mn,[h-2 for h in hoss],[h+2 for h in hoss],alpha=0.12,color=R,label='±2% field unc.')
    a1.set_xticks(mn); a1.set_xticklabels(mn_n,fontsize=6,rotation=30)
    a1.set_ylabel('MPPT Efficiency (%)'); a1.set_ylim(60,100)
    a1.legend(fontsize=6); a1.grid(True,alpha=0.35)
    a1.set_title('(a) Monthly comparison\nvs Hossain et al. IDCOL SHS field data')
    mm=[5,6,7,8,9,10]; mn2='May Jun Jul Aug Sep Oct'.split()
    xb=np.arange(6); wb=0.28
    a2.bar(xb-wb,[po[m-1] for m in mm],   wb,label='Plain P&O',  color=GR,alpha=0.8,edgecolor='white',lw=0.4)
    a2.bar(xb,  [hoss[m-1] for m in mm], wb,label='Hossain et al.',color=R,alpha=0.8,edgecolor='white',lw=0.4)
    a2.bar(xb+wb,[ours[m-1] for m in mm],wb,label='Helios-Artemis',color=B,alpha=0.8,edgecolor='white',lw=0.4)
    a2.set_xticks(xb); a2.set_xticklabels(mn2,fontsize=7)
    a2.set_ylabel('MPPT Efficiency (%)'); a2.set_ylim(60,100)
    a2.legend(fontsize=6); a2.grid(True,axis='y',alpha=0.35)
    a2.set_title('(b) Monsoon season\n(May–Oct, peak cloud variability)')
    fig.suptitle('Fig. 9.  Partial validation — simulated vs Hossain et al. IDCOL SHS field baseline',fontsize=8.5,fontweight='bold')
    fig.tight_layout(); fig.savefig(f'{OUT}/fig9_validation.png'); plt.close(); print('[OK] Fig 9')

if __name__=='__main__':
    fig9()
    print(f'Output: {OUT}/fig9_validation.png')