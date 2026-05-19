"""
Fig 2: Irradiance model - Markov + OU flicker for Sylhet July
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
np.random.seed(23)

def fig2():
    dt=0.1; sr=5.5; ss=18.5
    T=np.array([[0.6,0.3,0.1],[0.2,0.5,0.3],[0.1,0.2,0.7]]); cf=[0.15,0.55,0.90]
    N=int(86400/dt); th=np.linspace(0,24,N); GG=np.zeros(N)
    cs=1; ct=0; gf=0; dc=np.exp(-dt)
    for i in range(N):
        h=th[i]
        if h<sr or h>ss: continue
        ct+=dt
        if ct>=15:
            ct=0; r=np.random.rand(); row=T[cs]
            cs=0 if r<row[0] else (1 if r<row[0]+row[1] else 2)
        fr=(h-sr)/(ss-sr); Gc=744*np.sin(np.pi*fr); Gcf=Gc*cf[cs]
        sf=0.25*max(Gcf,10); gf=gf*dc+sf*np.sqrt(1-dc**2)*np.random.randn()
        gf=max(-0.4*Gc,min(0.4*Gc,gf)); GG[i]=min(Gc,max(0,Gcf+gf))
    Ge=np.array([744*np.sin(np.pi*(h-sr)/(ss-sr)) if sr<=h<=ss else 0 for h in th])

    fig,axes=plt.subplots(2,1,figsize=(C2,3.5))
    axes[0].fill_between(th,GG,alpha=0.55,color=B,label='Stochastic GHI')
    axes[0].plot(th,Ge,'r--',lw=0.9,label='Clear-sky envelope (aerosol ×0.93, peak 744 W m⁻²)')
    axes[0].set_ylabel('GHI (W m⁻²)'); axes[0].set_xlim(0,24); axes[0].set_ylim(0,800)
    axes[0].legend(); axes[0].grid(True)
    axes[0].set_title('(a) Full-day Sylhet Markov+OU irradiance — July (3-state: overcast/partly/clear)')
    m=(th>=9.5)&(th<=11.5)
    axes[1].plot(th[m],GG[m],color=B,lw=0.55,label='GHI with OU flicker (τ=1s, σ=25%)')
    axes[1].plot(th[m],Ge[m],'r--',lw=0.9,label='Clear-sky')
    axes[1].fill_between(th[m],GG[m],alpha=0.22,color=B)
    axes[1].set_xlabel('Time (h)'); axes[1].set_ylabel('GHI (W m⁻²)')
    axes[1].set_title('(b) Zoom 09:30–11:30 h — sub-second OU flicker visible (Lave & Kleissl 2010)')
    axes[1].legend(); axes[1].grid(True)
    fig.suptitle('Fig. 2.  Calibrated Sylhet irradiance model (Markov 15s state, OU flicker, aerosol attenuation)',fontsize=8.5,fontweight='bold')
    fig.tight_layout(); fig.savefig(f'{OUT}/fig2_irradiance.png'); plt.close(); print('[OK] Fig 2')

if __name__=='__main__':
    fig2()
    print(f'Output: {OUT}/fig2_irradiance.png')