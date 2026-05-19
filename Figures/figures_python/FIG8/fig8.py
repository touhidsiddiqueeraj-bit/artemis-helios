"""
Fig 8: Bill of materials and cost comparison
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

def fig8():
    fig,(a1,a2)=plt.subplots(1,2,figsize=(C2,2.9))
    comps={'ESP32-S3\nModule':380,'STM32F103\nBlue Pill':120,'INA219':80,
           'TSL2591':120,'Buck Stage\n(passives)':350,'PCB+Housing':280,'Misc':170}
    cols_=[B,'#5C6BC0',G,O,R,'#795548','#607D8B']
    vs=list(comps.values()); tot=sum(vs)
    w,_,atx=a1.pie(vs,labels=None,autopct='%1.0f%%',colors=cols_,startangle=90,
                    wedgeprops=dict(edgecolor='white',lw=0.6),pctdistance=0.78,
                    textprops={'fontsize':5.6})
    a1.legend(w,list(comps.keys()),loc='lower center',bbox_to_anchor=(0.5,-0.42),
              fontsize=5.5,ncol=2,framealpha=0.9)
    a1.set_title(f'(a) BOM breakdown\nTotal: {tot} BDT (≈ {tot/110:.0f} USD)')

    sys_=['Helios-Artemis\n(this work)','PWM Ctrl\n(low-end)','Basic MPPT\n(market)','IDCOL-compat\nMPPT']
    bdt=[tot,2800,5200,13500]; cc_=[G,GR,O,R]
    bars=a2.bar(np.arange(4),bdt,color=cc_,alpha=0.85,edgecolor='white',lw=0.5,width=0.6)
    a2.bar_label(bars,labels=[f'{v:,}' for v in bdt],fontsize=6,padding=2)
    a2.set_xticks(np.arange(4)); a2.set_xticklabels(sys_,fontsize=6)
    a2.set_ylabel('Retail Cost (BDT)'); a2.grid(True,axis='y',alpha=0.35)
    a2.set_title('(b) vs commercial alternatives\n(1 USD ≈ 110 BDT, Q1 2026)')
    a2.text(0,tot+350,f'↓{(1-tot/13500)*100:.0f}%\nvs IDCOL',ha='center',fontsize=6,color=G,fontweight='bold')
    fig.suptitle('Fig. 8.  Bill of materials and cost comparison (Dhaka retail Q1 2026)',fontsize=8.5,fontweight='bold')
    fig.tight_layout(); fig.savefig(f'{OUT}/fig8_cost.png'); plt.close(); print('[OK] Fig 8')

if __name__=='__main__':
    fig8()
    print(f'Output: {OUT}/fig8_cost.png')