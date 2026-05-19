function [sys,x0,str,ts]=ha_artemis_v3(t,x,u,flag)
% Artemis MPPT: variable-step P&O blended with NARX irradiance prediction
% ADC noise: sigma_I=2 mA, sigma_V=10 mV
% States: x(1)=Vref  x(2)=P_prev  x(3)=V_prev  x(4)=cooldown_counter
alpha_base=0.45; Vmp_nom=17.8; Voc_nom=21.6;
sigma_I=0.002; sigma_V=0.010;
switch flag
case 0
 s=simsizes; s.NumContStates=0; s.NumDiscStates=4;
 s.NumOutputs=3; s.NumInputs=6; s.DirFeedthrough=1; s.NumSampleTimes=1;
 sys=simsizes(s); x0=[Vmp_nom;0;Vmp_nom;0]; str=[]; ts=[0.1 0];
case 2
 Vr=x(1); Pp=x(2); Vp_prev=x(3); cool=x(4);
 Im=max(0,u(1)+sigma_I*randn); Vm=max(0.1,u(2)+sigma_V*randn);
 P=Im*Vm; dP=P-Pp; dV=Vm-Vp_prev;
 dl=min(max(0.008*abs(dP/(abs(dV)+1e-9)),0.05),0.60);
 if dP>=0; if dV>=0, Vr=Vr+dl; else, Vr=Vr-dl; end
 else; if dV>=0, Vr=Vr-dl; else, Vr=Vr+dl; end; end
 Gp=u(3); Gn=u(4); cool=max(0,cool-1);
 if cool==0 && Gn>80 && Gp>80
  rel_dev=abs(Gp-Gn)/max(max(Gp,Gn),10);
  if rel_dev>0.15
   if Gp>Gn, alpha=alpha_base*exp(-1.5*rel_dev);
   else, alpha=0.08*exp(-1.0*rel_dev); end
   Vmppt=Vmp_nom*(1-0.014*log(max(Gp,1)/1000));
   Vr=(1-alpha)*Vr+alpha*Vmppt; cool=20;
  end
 end
 Vr=min(max(Vr, 0.60*Voc_nom), 0.98*Voc_nom);
 sys=[Vr; P; Vm; cool];
case 3
 Pm=max(u(6),0.01); Pcurr=max(0,u(1)*u(2));
 eta=min(1.0, Pcurr/Pm);
 if u(5)<14.70, cs=1; elseif u(5)<14.76, cs=2; else, cs=3; end
 sys=[x(1); eta; cs];
otherwise, sys=[];
end
