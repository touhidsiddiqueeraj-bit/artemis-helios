function [sys,x0,str,ts]=ha_pv_v3(t,x,u,flag)
% 50 Wp mono-Si panel, IEC 61215 one-diode model
% Soiling factor 3%, NOCT=45 C, fitted exponent k=14.2606
% such that I(Vmp) == Imp exactly at STC
Voc0=21.6; Vmp0=17.8; Isc0=3.0*0.97; Imp0=2.81;
k=14.2606;
bVoc=-3.4e-3; bVmp=-4.1e-3; aIsc=0.5e-3;
NOCT=45; Tamb=35; kJ=0.04;
switch flag
case 0
 s=simsizes; s.NumContStates=0; s.NumDiscStates=0;
 s.NumOutputs=4; s.NumInputs=3; s.DirFeedthrough=1; s.NumSampleTimes=1;
 sys=simsizes(s); x0=[]; str=[]; ts=[0.1 0];
case 3
 G=max(0,u(1)); Vref=u(2); Tj_ext=u(3);
 g=G/1000;
 Tc=Tamb+(NOCT-20)/800*G+kJ*max(0,Tj_ext-40);
 dT=Tc-25;
 Voc_T=Voc0*(1+bVoc*dT);
 Vmp_T=Vmp0*(1+bVmp*dT);
 Isc_T=Isc0*(1+aIsc*dT)*g;
 Imp_T=Imp0*g;
 Vr=min(max(Vref,0.1), Voc_T-0.05);
 Ipv=max(0, Isc_T*(1-(Vr/Voc_T)^k));
 Ppv=Ipv*Vr;
 Vmp_c=Voc_T*(1/(1+k))^(1/k);
 Pm=max(0, Isc_T*(1-(Vmp_c/Voc_T)^k)*Vmp_c);
 sys=[Ipv; Vr; Ppv; Pm];
otherwise, sys=[];
end
