function [sys,x0,str,ts]=ha_irr_v3(t,x,u,flag)
% Sylhet Markov cloud model (15 s) + Ornstein-Uhlenbeck flicker
% (tau=1 s, sigma=25%) + aerosol attenuation (0.93)
% States: x(1)=cloud_state  x(2)=cloud_timer  x(3)=G_fast
T_row1=[0.60 0.30 0.10]; T_row2=[0.20 0.50 0.30]; T_row3=[0.10 0.20 0.70];
cf=[0.15 0.55 0.90]; sunrise=5.5; sunset=18.5;
tau_f=1.0; dt=0.1; sigma_frac=0.25; aerosol=0.93; decay=exp(-dt/tau_f);
switch flag
case 0
 s=simsizes; s.NumContStates=0; s.NumDiscStates=3;
 s.NumOutputs=1; s.NumInputs=0; s.DirFeedthrough=0; s.NumSampleTimes=1;
 sys=simsizes(s); x0=[1;0;0]; str=[]; ts=[0.1 0];
case 2
 cs=x(1); ctimer=x(2); G_fast=x(3);
 t_h=mod(t,86400)/3600;
 if t_h>=sunrise && t_h<=sunset
  ctimer=ctimer+dt;
  if ctimer>=15
   ctimer=0; r=rand;
   if cs==0, row=T_row1; elseif cs==1, row=T_row2; else, row=T_row3; end
   if r<row(1), cs=0; elseif r<row(1)+row(2), cs=1; else, cs=2; end
  end
  frac=(t_h-sunrise)/(sunset-sunrise);
  Gclear=800*aerosol*sin(pi*frac); Gcf=Gclear*cf(cs+1);
  sigma_f=sigma_frac*max(Gcf,10);
  G_fast=G_fast*decay+sigma_f*sqrt(1-decay^2)*randn;
  G_fast=max(-0.40*Gclear, min(0.40*Gclear, G_fast));
 else
  ctimer=0; G_fast=0;
 end
 sys=[cs; ctimer; G_fast];
case 3
 t_h=mod(t,86400)/3600;
 if t_h<sunrise || t_h>sunset, sys=0; return; end
 cs=x(1); G_fast=x(3);
 frac=(t_h-sunrise)/(sunset-sunrise);
 Gclear=800*aerosol*sin(pi*frac); Gcf=Gclear*cf(cs+1);
 sys=min(Gclear, max(0, Gcf+G_fast));
otherwise, sys=[];
end
