"""
Fig 3: I-V and P-V curves for 50Wp mono-Si PV panel
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

def pv_model(G, Vr, Isc0=2.91, Voc0=21.6, k=14.2606,
             bV=-3.4e-3, aI=0.5e-3, NOCT=45, Tamb=35):
    g=G/1000; Tc=Tamb+(NOCT-20)/800*G; dT=Tc-25
    VocT=Voc0*(1+bV*dT); IscT=Isc0*(1+aI*dT)*g
    Vmc=VocT*(1/(1+k))**(1/k)
    Pm=IscT*(1-(Vmc/VocT)**k)*Vmc
    Vrc=min(max(Vr,0.6*Voc0),0.98*Voc0)
    Ipv=max(0,IscT*(1-(Vrc/VocT)**k))
    return Ipv*Vrc, Pm, Vrc

def fig3():
    fig,(a1,a2)=plt.subplots(1,2,figsize=(C2,2.9))
    cols_=[B,'#0288D1',G,O,R]; Gvs=[200,400,600,800,1000]
    for Gv,col in zip(Gvs,cols_):
        g=Gv/1000; Tc=35+(45-20)/800*Gv; dT=Tc-25
        VocT=21.6*(1-3.4e-3*dT); IscT=2.91*(1+0.5e-3*dT)*g; k=14.2606
        Vmc=VocT*(1/(1+k))**(1/k); Pm=IscT*(1-(Vmc/VocT)**k)*Vmc
        V=np.linspace(0,VocT*0.999,400); I=np.maximum(0,IscT*(1-(V/VocT)**k))
        a1.plot(V,I,color=col,lw=0.85,label=f'{Gv} W m⁻²')
        a1.plot(Vmc,IscT*(1-(Vmc/VocT)**k),'o',color=col,ms=3.5,zorder=5)
        a2.plot(V,I*V,color=col,lw=0.85,label=f'Pm={Pm:.1f}W')
        a2.plot(Vmc,Pm,'o',color=col,ms=3.5,zorder=5)
    a1.set_xlabel('Voltage (V)'); a1.set_ylabel('Current (A)')
    a1.set_title('(a) I–V — dots = true MPP\n(soiling 3%, Tamb=35°C, NOCT=45°C)')
    a1.legend(fontsize=6.5); a1.grid(True); a1.set_xlim(0,22); a1.set_ylim(0,3.1)
    a2.set_xlabel('Voltage (V)'); a2.set_ylabel('Power (W)')
    a2.set_title('(b) P–V — Vmp from exact dP/dV=0\n(analytical solution, k=14.26)')
    a2.legend(fontsize=6.5,title='Peak powers'); a2.grid(True); a2.set_xlim(0,22)
    fig.suptitle('Fig. 3.  PV model I–V and P–V characteristics (50Wp mono-Si, Isc₀=2.91A post-soiling)',fontsize=8.5,fontweight='bold')
    fig.tight_layout(); fig.savefig(f'{OUT}/fig3_iv_curves.png'); plt.close(); print('[OK] Fig 3')

if __name__=='__main__':
    fig3()
    print(f'Output: {OUT}/fig3_iv_curves.png')