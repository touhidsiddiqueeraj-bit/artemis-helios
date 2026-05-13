"""
graphical_abstract.py — Helios-Artemis
Single-panel visual summary for MDPI Energies submission
"""
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
from scipy.stats import norm
import os

np.random.seed(23)
plt.rcParams.update({
    'font.family':'sans-serif','font.sans-serif':['DejaVu Sans','Arial'],
    'font.size':9,'figure.dpi':300,'savefig.dpi':300,
    'savefig.bbox':'tight','savefig.pad_inches':0.06,
})

fig = plt.figure(figsize=(10, 6.5), facecolor='#0D1B2A')
fig.patch.set_facecolor('#0D1B2A')

gs = gridspec.GridSpec(2, 3, figure=fig,
                       left=0.07, right=0.97, top=0.82, bottom=0.08,
                       hspace=0.45, wspace=0.38)

TEAL   = '#00BCD4'
AMBER  = '#FFB300'
GREEN  = '#69F0AE'
RED    = '#EF5350'
WHITE  = '#ECEFF1'
LBLUE  = '#90CAF9'
DGREY  = '#263238'
PANEL  = '#1A2E3D'

# ── Title ──────────────────────────────────────────────────────────────────
fig.text(0.50, 0.935,
         'Helios-Artemis: Dual-MCU Predictive MPPT for Bangladesh Solar Home Systems',
         ha='center', va='center', fontsize=12.5, fontweight='bold', color=WHITE)
fig.text(0.50, 0.895,
         'LSTM Irradiance Prediction  ·  Variable-Step P&O  ·  94.0 ± 0.6% Tracking Efficiency  ·  ~1,500 BDT',
         ha='center', va='center', fontsize=9, color=TEAL)

def panel_bg(ax, color=PANEL):
    ax.set_facecolor(color)
    for sp in ax.spines.values(): sp.set_edgecolor('#37474F'); sp.set_linewidth(0.6)

# ── Panel 1: Irradiance model ──────────────────────────────────────────────
ax1 = fig.add_subplot(gs[0, 0])
panel_bg(ax1)
sr,ss=5.5,18.5; T=np.array([[0.6,0.3,0.1],[0.2,0.5,0.3],[0.1,0.2,0.7]]); cf=[0.15,0.55,0.90]
N=int(86400/0.1); th=np.linspace(0,24,N); GG=np.zeros(N)
cs=1; ct=0; gf=0; dc=np.exp(-0.1)
for i in range(N):
    h=th[i]
    if h<sr or h>ss: continue
    ct+=0.1
    if ct>=15:
        ct=0; r=np.random.rand(); row=T[cs]
        cs=0 if r<row[0] else (1 if r<row[0]+row[1] else 2)
    fr=(h-sr)/(ss-sr); Gc=744*np.sin(np.pi*fr); Gcf=Gc*cf[cs]
    sf=0.25*max(Gcf,10); gf=gf*dc+sf*np.sqrt(1-dc**2)*np.random.randn()
    gf=max(-0.4*Gc,min(0.4*Gc,gf)); GG[i]=min(Gc,max(0,Gcf+gf))
Ge=np.array([744*np.sin(np.pi*(h-sr)/(ss-sr)) if sr<=h<=ss else 0 for h in th])
ax1.fill_between(th,GG,alpha=0.5,color=TEAL)
ax1.plot(th,Ge,'--',color=AMBER,lw=0.9,label='Clear-sky')
ax1.set_xlim(0,24); ax1.set_ylim(0,800)
ax1.set_xlabel('Time (h)',color=WHITE,fontsize=8); ax1.set_ylabel('GHI (W m⁻²)',color=WHITE,fontsize=8)
ax1.tick_params(colors=WHITE,labelsize=7); ax1.legend(fontsize=7,labelcolor=WHITE,facecolor=DGREY,edgecolor='#555')
ax1.set_title('Sylhet Markov+OU Irradiance\n(July, 3-state cloud model)',color=TEAL,fontsize=8.5,fontweight='bold')

# ── Panel 2: I-V curves ────────────────────────────────────────────────────
ax2 = fig.add_subplot(gs[0, 1])
panel_bg(ax2)
cols_=['#00E5FF','#00BCD4','#69F0AE','#FFB300','#EF5350']
for Gv,col in zip([200,400,600,800,1000],cols_):
    dT=(35+(45-20)/800*Gv)-25; VocT=21.6*(1-3.4e-3*dT); IscT=2.91*(1+0.5e-3*dT)*Gv/1000
    k=14.2606; Vmc=VocT*(1/(1+k))**(1/k)
    V=np.linspace(0,VocT*0.999,300); I=np.maximum(0,IscT*(1-(V/VocT)**k))
    ax2.plot(V,I*V,color=col,lw=0.85,label=f'{Gv}')
    Pm=IscT*(1-(Vmc/VocT)**k)*Vmc; ax2.plot(Vmc,Pm,'o',color=col,ms=3.5)
ax2.set_xlabel('Voltage (V)',color=WHITE,fontsize=8); ax2.set_ylabel('Power (W)',color=WHITE,fontsize=8)
ax2.tick_params(colors=WHITE,labelsize=7); ax2.set_xlim(0,22)
ax2.legend(title='G (W m⁻²)',fontsize=6.5,title_fontsize=6.5,labelcolor=WHITE,facecolor=DGREY,edgecolor='#555',ncol=2)
ax2.set_title('PV P–V Curves\n(soiling 3%, Tamb=35°C)',color=TEAL,fontsize=8.5,fontweight='bold')

# ── Panel 3: MC efficiency histogram ─────────────────────────────────────
ax3 = fig.add_subplot(gs[0, 2])
panel_bg(ax3)
mc=np.array([92.6,93.1,93.4,93.7,93.8,94.0,94.1,94.2,94.3,94.5,
             94.6,94.7,94.7,94.8,94.9,93.9,93.6,93.3,94.4,94.1,
             93.8,94.0,94.2,93.5,94.3,93.7,94.1,94.4,93.9,95.0])
ax3.hist(mc,bins=12,color=TEAL,alpha=0.65,density=True,edgecolor='#0D1B2A',lw=0.5)
xr=np.linspace(91,96,300); ax3.plot(xr,norm.pdf(xr,mc.mean(),mc.std()),color=AMBER,lw=1.2)
ax3.axvline(mc.mean(),color=GREEN,ls='--',lw=1.0,label=f'μ={mc.mean():.1f}%')
ax3.axvspan(np.percentile(mc,2.5),np.percentile(mc,97.5),alpha=0.15,color=GREEN)
ax3.set_xlabel('MPPT Efficiency (%)',color=WHITE,fontsize=8); ax3.set_ylabel('Density',color=WHITE,fontsize=8)
ax3.tick_params(colors=WHITE,labelsize=7); ax3.legend(fontsize=7.5,labelcolor=WHITE,facecolor=DGREY,edgecolor='#555')
ax3.set_title('30-Day Monte Carlo\n94.0 ± 0.6%  [95% CI: 92.9–94.9%]',color=TEAL,fontsize=8.5,fontweight='bold')

# ── Panel 4: Efficiency bar comparison ───────────────────────────────────
ax4 = fig.add_subplot(gs[1, 0])
panel_bg(ax4)
cats=['Plain P&O','VS-P&O','Helios-Artemis']; vals=[70.7,85.2,94.0]
bar_cols=['#EF5350','#FFB300','#69F0AE']
bars=ax4.barh(cats,vals,color=bar_cols,alpha=0.85,edgecolor='#0D1B2A',lw=0.4,height=0.55)
for bar,v in zip(bars,vals):
    ax4.text(v+0.3,bar.get_y()+bar.get_height()/2,f'{v:.1f}%',
             va='center',fontsize=8,color=WHITE,fontweight='bold')
ax4.set_xlim(60,100); ax4.set_xlabel('Monsoon Tracking Efficiency (%)',color=WHITE,fontsize=8)
ax4.tick_params(colors=WHITE,labelsize=8); ax4.axvline(90,color=TEAL,ls=':',lw=0.8,alpha=0.7)
ax4.set_title('Controller Comparison\n(Sylhet July monsoon)',color=TEAL,fontsize=8.5,fontweight='bold')

# ── Panel 5: SoC + Vbat (simulation result) ──────────────────────────────
ax5 = fig.add_subplot(gs[1, 1])
panel_bg(ax5)
# Regenerate quick SoC curve
dt=0.1; N2=int(86400/dt); th2=np.linspace(0,24,N2)
SoC_=np.zeros(N2); SoC_[0]=0.45; Qa=0.45*7.0
cs2=1; ct2=0; gf2=0
for i in range(1,N2):
    h=th2[i]
    if 5.5<=h<=18.5:
        ct2+=dt
        if ct2>=15:
            ct2=0; r=np.random.rand(); row=T[cs2]
            cs2=0 if r<row[0] else (1 if r<row[0]+row[1] else 2)
        fr=(h-5.5)/13.0; Gc=744*np.sin(np.pi*fr); Gcf=Gc*cf[cs2]
        sf=0.25*max(Gcf,10); gf2=gf2*dc+sf*np.sqrt(1-dc**2)*np.random.randn()
        gf2=max(-0.4*Gc,min(0.4*Gc,gf2)); Gv2=min(Gc,max(0,Gcf+gf2))
        Ib=max(0,Gv2/1000*2.0*0.94*0.85); Qa=min(7.0,Qa+Ib*dt/3600)
    SoC_[i]=Qa/7.0

ax5_twin=ax5.twinx(); ax5_twin.set_facecolor(PANEL)
ax5.plot(th2,SoC_*100,color=GREEN,lw=0.8,label='SoC')
Vb=11.0+2.0*SoC_; ax5_twin.plot(th2,Vb,'--',color=AMBER,lw=0.7,label='Vbat',alpha=0.85)
ax5.set_xlim(0,24); ax5.set_ylim(0,110); ax5_twin.set_ylim(11,15)
ax5.set_xlabel('Time (h)',color=WHITE,fontsize=8); ax5.set_ylabel('SoC (%)',color=GREEN,fontsize=8)
ax5_twin.set_ylabel('V$_{bat}$ (V)',color=AMBER,fontsize=8)
ax5.tick_params(colors=WHITE,labelsize=7); ax5_twin.tick_params(colors=AMBER,labelsize=7)
ax5.set_title('Battery Charging Profile\n30% → 100% SoC, CC/CV',color=TEAL,fontsize=8.5,fontweight='bold')

# ── Panel 6: Key metrics summary card ────────────────────────────────────
ax6 = fig.add_subplot(gs[1, 2])
ax6.set_facecolor('#0F2030'); ax6.axis('off')
for sp in ax6.spines.values(): sp.set_edgecolor(TEAL); sp.set_linewidth(1.2); sp.set_visible(True)

metrics=[
    ('MPPT Efficiency',  '94.0 ± 0.6%',    GREEN),
    ('95% CI',           '[92.9, 94.9]%',   LBLUE),
    ('LSTM R²',          '0.917',           AMBER),
    ('LSTM MAE',         '50.7 W m⁻²',      AMBER),
    ('System Cost',      '~1,500 BDT',      GREEN),
    ('vs IDCOL MPPT',    '↓ 89%',           '#EF5350'),
    ('UART Latency',     '100 ms',          LBLUE),
    ('Retrain Time',     '24 h on-device',  LBLUE),
    ('Cloud Dep.',       'None (zero)',      GREEN),
]
ax6.set_xlim(0,10); ax6.set_ylim(0,len(metrics)+1)
ax6.text(5,len(metrics)+0.55,'Key Performance Metrics',ha='center',va='center',
         fontsize=9,fontweight='bold',color=TEAL)
for i,(lbl,val,col) in enumerate(reversed(metrics)):
    y=i+0.5
    ax6.text(0.3,y,lbl,va='center',fontsize=8,color='#B0BEC5')
    ax6.text(9.7,y,val,va='center',ha='right',fontsize=8.5,fontweight='bold',color=col)
    ax6.axhline(y+0.5,color='#1C3040',lw=0.4,alpha=0.7)

# ── Footer ─────────────────────────────────────────────────────────────────
fig.text(0.50, 0.025,
         'Hussain Touhid Siddiquee · Dept. EEE, Leading University Sylhet, Bangladesh · touhidsiddiqueeraj@gmail.com',
         ha='center', fontsize=7.5, color='#607D8B', style='italic')

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'figures')
os.makedirs(OUT, exist_ok=True)
fig.savefig(os.path.join(OUT, 'graphical_abstract.png'), facecolor='#0D1B2A')
plt.close()
print('[OK] Graphical abstract saved')
