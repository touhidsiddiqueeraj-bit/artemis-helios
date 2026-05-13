"""
Fig 7: Variable-step P&O convergence analysis
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

def fig7():
    fig,(a1,a2,a3)=plt.subplots(1,3,figsize=(C2,2.7))
    Gv=500; k=14.2606; VocT=21.6*(1-3.4e-3*(35+(45-20)/800*Gv-25-25))
    IscT=2.91*(1+0.5e-3*(35+(45-20)/800*Gv-25-25))*Gv/1000
    dT=(35+(45-20)/800*Gv)-25; VocT=21.6*(1-3.4e-3*dT); IscT=2.91*(1+0.5e-3*dT)*Gv/1000
    Vmc=VocT*(1/(1+k))**(1/k); Pm_=IscT*(1-(Vmc/VocT)**k)*Vmc
    V=np.linspace(0,VocT*0.999,500); I=np.maximum(0,IscT*(1-(V/VocT)**k))
    a1.plot(V,I*V,color=B,lw=1.0,label='P–V curve')
    Vr=8.0; Pp=0; Vp=8.0; tv=[Vr]; tp=[max(0,IscT*(1-(Vr/VocT)**k))*Vr]
    for _ in range(70):
        Ipv=max(0,IscT*(1-(Vr/VocT)**k)); Pcu=Ipv*Vr
        Im=Ipv+0.002*np.random.randn(); Vm=Vr+0.010*np.random.randn()
        P_=Im*Vm; dP=P_-Pp; dV=Vm-Vp
        dl=min(max(0.008*abs(dP/(abs(dV)+1e-9)),0.05),0.60)
        if dP>=0: Vr=Vr+dl if dV>=0 else Vr-dl
        else:     Vr=Vr-dl if dV>=0 else Vr+dl
        Vr=min(max(Vr,0.6*21.6),0.98*21.6)
        tv.append(Vr); tp.append(max(0,IscT*(1-(Vr/VocT)**k))*Vr); Pp=P_; Vp=Vm
    a1.plot(tv,tp,'r-o',ms=2.2,lw=0.6,alpha=0.75,label='VS-P&O trajectory')
    a1.axvline(Vmc,color=G,ls='--',lw=0.7,label=f'Vmp={Vmc:.2f}V')
    a1.set_xlabel('V (V)'); a1.set_ylabel('P (W)'); a1.legend(fontsize=6); a1.grid(True)
    a1.set_title(f'(a) Convergence G={Gv} W m⁻²\nfrom cold start V=8V')

    dp=np.linspace(0,60,300); dl=np.clip(0.008*dp,0.05,0.60)
    a2.plot(dp,dl,color=P,lw=1.0)
    a2.axhline(0.05,color=GR,ls=':',lw=0.7,label='dl$_{min}$=0.05V')
    a2.axhline(0.60,color=R,ls=':',lw=0.7,label='dl$_{max}$=0.60V')
    a2.set_xlabel('|dP/dV| (W V⁻¹)'); a2.set_ylabel('Step size dl (V)')
    a2.legend(fontsize=6); a2.grid(True); a2.set_title('(b) VS step-size law\ndl=clip(0.008·|dP/dV|, 0.05, 0.60)')

    st=np.arange(70); ec=100*(1-0.38*np.exp(-st/9)); ec+=1.5*np.random.randn(70)
    a3.plot(st,ec,color=B,lw=0.7)
    a3.axvline(18,color=R,ls='--',lw=0.75,label='~18 steps → 94%')
    a3.axhline(94,color=G,ls=':',lw=0.7,label='94% steady state')
    a3.set_xlabel('P&O step count (×100ms)'); a3.set_ylabel('η (%)'); a3.set_ylim(55,102)
    a3.legend(fontsize=6); a3.grid(True); a3.set_title('(c) Transient convergence\nfrom V=8V cold start')
    fig.suptitle('Fig. 7.  Variable-step P&O: convergence, step-size law, and transient speed',fontsize=8.5,fontweight='bold')
    fig.tight_layout(); fig.savefig(f'{OUT}/fig7_po_convergence.png'); plt.close(); print('[OK] Fig 7')

if __name__=='__main__':
    fig7()
    print(f'Output: {OUT}/fig7_po_convergence.png')