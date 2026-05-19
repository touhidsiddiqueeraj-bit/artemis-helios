function [sys,x0,str,ts]=ha_battery_v3(t,x,u,flag)
% 7 Ah / 12 V SLA battery: Shepherd OCV, Peukert capacity loss, Rint=50 mohm
% States: x(1)=charge_Ah  x(2)=terminal_voltage (warm-start avoids t=0 spike)
Qnom=7.0; Irate=1.0; np=1.25; Rint=0.05;
SoC0=0.30;
Voc0_=11.84+1.98*SoC0-0.28*SoC0^2;
Vt0_=Voc0_;
switch flag
case 0
 s=simsizes; s.NumContStates=0; s.NumDiscStates=2;
 s.NumOutputs=2; s.NumInputs=1; s.DirFeedthrough=0; s.NumSampleTimes=1;
 sys=simsizes(s); x0=[SoC0*Qnom; Vt0_]; str=[]; ts=[0.1 0];
case 2
 Q=x(1); Ic=max(0,u(1));
 dQ=(Ic/max(Irate,0.01))^(np-1)*Ic*0.1/3600;
 Q_new=min(Q+dQ, Qnom);
 SoC_new=Q_new/Qnom;
 Voc_new=11.84+1.98*SoC_new-0.28*SoC_new^2;
 Vt_new=Voc_new+Rint*Ic;
 sys=[Q_new; Vt_new];
case 3
 SoC=x(1)/Qnom;
 sys=[x(2); SoC];
otherwise, sys=[];
end
