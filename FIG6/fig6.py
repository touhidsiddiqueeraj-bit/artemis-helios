"""
Fig 6: MPPT efficiency comparison and Monte Carlo distribution
"""
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from scipy.stats import norm
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

def fig6():
    fig,(a1,a2)=plt.subplots(1,2,figsize=(C2,3.0))
    methods=['Plain P&O','VS-P&O\n(no LSTM)','Helios-Artemis\n(this work)']
    mon=[70.7,85.2,94.0]; ann=[85.8,88.1,94.0]; x=np.arange(3); w=0.36
    b1=a1.bar(x-w/2,mon,w,label='Monsoon July',color=B,alpha=0.85,edgecolor='white',lw=0.4)
    b2=a1.bar(x+w/2,ann,w,label='Annual equiv.',color=G,alpha=0.85,edgecolor='white',lw=0.4)
    a1.bar_label(b1,fmt='%.1f%%',fontsize=5.8,padding=2); a1.bar_label(b2,fmt='%.1f%%',fontsize=5.8,padding=2)
    a1.set_xticks(x); a1.set_xticklabels(methods,fontsize=6.5)
    a1.set_ylabel('MPPT Efficiency (%)'); a1.set_ylim(60,104)
    a1.axhspan(90,95,alpha=0.10,color=O,label='Literature range'); a1.legend(fontsize=6)
    a1.grid(True,axis='y',alpha=0.35); a1.set_title('(a) Method comparison\n(Monte Carlo mean)')

    mc=np.array([92.6,93.1,93.4,93.7,93.8,94.0,94.1,94.2,94.3,94.5,
                 94.6,94.7,94.7,94.8,94.9,93.9,93.6,93.3,94.4,94.1,
                 93.8,94.0,94.2,93.5,94.3,93.7,94.1,94.4,93.9,95.0])
    a2.hist(mc,bins=12,color=B,alpha=0.65,density=True,edgecolor='white',lw=0.3)
    xr=np.linspace(91,96,300); a2.plot(xr,norm.pdf(xr,mc.mean(),mc.std()),color=R,lw=1.0,label='Gaussian')
    a2.axvline(mc.mean(),color='k',ls='--',lw=0.85,label=f'μ={mc.mean():.1f}%')
    ci_lo,ci_hi=np.percentile(mc,2.5),np.percentile(mc,97.5)
    a2.axvspan(ci_lo,ci_hi,alpha=0.12,color=G,label=f'95% CI [{ci_lo:.1f},{ci_hi:.1f}]%')
    a2.set_xlabel('MPPT Efficiency (%)'); a2.set_ylabel('Density')
    a2.legend(fontsize=6); a2.grid(True,alpha=0.35)
    a2.set_title(f'(b) 30-day Monte Carlo\nμ={mc.mean():.1f}%, σ={mc.std():.2f}%')
    fig.suptitle('Fig. 6.  MPPT efficiency comparison and Monte Carlo distribution (N=30 days)',fontsize=8.5,fontweight='bold')
    fig.tight_layout(); fig.savefig(f'{OUT}/fig6_comparison.png'); plt.close(); print('[OK] Fig 6')

if __name__=='__main__':
    fig6()
    print(f'Output: {OUT}/fig6_comparison.png')