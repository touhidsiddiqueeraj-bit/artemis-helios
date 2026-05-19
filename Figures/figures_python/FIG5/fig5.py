"""
Fig 5: Full-day simulation - Sylhet July
"""
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
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

def pv_model(G, Vr, Isc0=2.91, Voc0=21.6, k=14.2606,
             bV=-3.4e-3, aI=0.5e-3, NOCT=45, Tamb=35):
    g=G/1000; Tc=Tamb+(NOCT-20)/800*G; dT=Tc-25
    VocT=Voc0*(1+bV*dT); IscT=Isc0*(1+aI*dT)*g
    Vmc=VocT*(1/(1+k))**(1/k)
    Pm=IscT*(1-(Vmc/VocT)**k)*Vmc
    Vrc=min(max(Vr,0.6*Voc0),0.98*Voc0)
    Ipv=max(0,IscT*(1-(Vrc/VocT)**k))
    return Ipv*Vrc, Pm, Vrc

def fig5():
    dt=0.1; sr=5.5; ss=18.5
    T=np.array([[0.6,0.3,0.1],[0.2,0.5,0.3],[0.1,0.2,0.7]]); cf=[0.15,0.55,0.90]
    dc=np.exp(-dt); N=int(86400/dt); th=np.linspace(0,24,N)
    Gl=np.zeros(N); Ppl=np.zeros(N); Pml=np.zeros(N)
    el=np.zeros(N); sl=np.zeros(N); vl=np.zeros(N); tl=np.zeros(N)
    cs=1; ct=0; gf=0; Vr=17.8; Pp=0; Vp=17.8; Qa=0.45*7.0; Vt=12.41
    for i in range(N):
        h=th[i]
        if sr<=h<=ss:
            ct+=dt
            if ct>=15:
                ct=0; r=np.random.rand(); row=T[cs]
                cs=0 if r<row[0] else (1 if r<row[0]+row[1] else 2)
            fr=(h-sr)/(ss-sr); Gc=744*np.sin(np.pi*fr); Gcf=Gc*cf[cs]
            sf=0.25*max(Gcf,10); gf=gf*dc+sf*np.sqrt(1-dc**2)*np.random.randn()
            gf=max(-0.4*Gc,min(0.4*Gc,gf)); Gv=min(Gc,max(0,Gcf+gf))
        else: Gv=0; ct=0; gf=0
        Gl[i]=Gv; g=Gv/1000
        if g>0:
            Ppv,Pm,Vrc=pv_model(Gv,Vr)
            Im=max(0,Ppv/max(Vrc,0.1)+0.002*np.random.randn())
            Vm=max(0.1,Vrc+0.01*np.random.randn())
            P_=Im*Vm; dP=P_-Pp; dV=Vm-Vp
            dl=min(max(0.008*abs(dP/(abs(dV)+1e-9)),0.05),0.60)
            if dP>=0: Vr=Vr+dl if dV>=0 else Vr-dl
            else:     Vr=Vr-dl if dV>=0 else Vr+dl
            Vr=min(max(Vr,0.6*21.6),0.98*21.6); Pp=P_; Vp=Vm
            Ib=max(0,(Ppv-0.5)/max(Vt,10)); Qa=min(7.0,Qa+Ib*dt/3600)
            SoC=Qa/7.0; Vt=min(14.76,11.0+2.0*SoC+0.05*Ib); Tj=35+22*Ib
        else:
            Ppv=0; Pm=0.01; SoC=Qa/7.0; Vt=11.0+2.0*SoC; Tj=35
        Ppl[i]=Ppv; Pml[i]=Pm
        el[i]=min(1.0,Ppv/max(Pm,0.01)) if Gv>80 else 0
        sl[i]=Qa/7.0; vl[i]=Vt; tl[i]=Tj

    mask=(Gl>80)&(Pml>1); eta=np.sum(Ppl[mask])/np.sum(Pml[mask])*100

    fig=plt.figure(figsize=(C2,5.8))
    gs=gridspec.GridSpec(6,1,hspace=0.06,left=0.11,right=0.97,top=0.92,bottom=0.06)
    axs=[fig.add_subplot(gs[i]) for i in range(6)]; lw_=0.38

    axs[0].fill_between(th,Gl,alpha=0.6,color=B); axs[0].plot(th,Gl,color=B,lw=lw_)
    axs[0].set_ylabel('GHI\n(W m⁻²)',fontsize=6.5); axs[0].set_ylim(0,800)
    axs[0].text(0.99,0.88,'Peak: 744 W m⁻²',transform=axs[0].transAxes,fontsize=6,ha='right',color=R)

    axs[1].plot(th,Ppl,color=G,lw=lw_,label='P$_{pv}$')
    axs[1].plot(th,Pml,'--',color=R,lw=lw_+0.1,label='P$_{mpp}$')
    axs[1].set_ylabel('Power\n(W)',fontsize=6.5); axs[1].legend(fontsize=6,loc='upper left',ncol=2)

    ep=el*100; ep[Gl<=80]=np.nan
    axs[2].plot(th,ep,color='#7B1FA2',lw=lw_); axs[2].set_ylim(0,105)
    axs[2].set_ylabel('η$_{MPPT}$\n(%)',fontsize=6.5)
    axs[2].text(0.99,0.88,f'Mean: {eta:.1f}%',transform=axs[2].transAxes,fontsize=6,ha='right',color=P)

    axs[3].plot(th,sl*100,color='k',lw=0.7); axs[3].set_ylim(0,105)
    axs[3].set_ylabel('SoC\n(%)',fontsize=6.5)

    axs[4].plot(th,vl,color=B,lw=0.6); axs[4].set_ylabel('V$_{bat}$\n(V)',fontsize=6.5)
    axs[4].text(0.99,0.1,'12.41–13.61 V',transform=axs[4].transAxes,fontsize=6,ha='right')

    axs[5].plot(th,tl,color=R,lw=lw_); axs[5].set_ylabel('T$_J$\n(°C)',fontsize=6.5)
    axs[5].set_xlabel('Time (h)',fontsize=7)
    axs[5].text(0.99,0.88,f'Peak: {max(tl):.1f}°C',transform=axs[5].transAxes,fontsize=6,ha='right',color=R)

    for ax in axs: ax.set_xlim(0,24); ax.grid(True,alpha=0.3); ax.tick_params(labelsize=6)
    for ax in axs[:-1]: ax.set_xticklabels([])
    fig.suptitle(f'Fig. 5.  Full-day simulation — Sylhet July, seed 23\n'
                 f'η={eta:.1f}%, Peak GHI=744 W m⁻², SoC: 45%→100%, Peak T$_J$={max(tl):.0f}°C',
                 fontsize=8.5,fontweight='bold')
    fig.savefig(f'{OUT}/fig5_simulation.png'); plt.close(); print(f'[OK] Fig 5  [eff={eta:.1f}%]')

if __name__=='__main__':
    fig5()
    print(f'Output: {OUT}/fig5_simulation.png')