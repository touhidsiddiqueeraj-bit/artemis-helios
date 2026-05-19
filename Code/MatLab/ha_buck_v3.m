function [sys,x0,str,ts]=ha_buck_v3(t,x,u,flag)
% IRFB4110 synchronous buck: D=Vbat/Vin, conduction+switching+DCR losses
Rds=3.7e-3; L_DCR=50e-3; Vf=0.45;
Coss=83e-9; Qg=10e-9; Vgs=10; tr=20e-9; fsw=50e3;
Rth=18.55; Tamb=40;
switch flag
case 0
 s=simsizes; s.NumContStates=0; s.NumDiscStates=0;
 s.NumOutputs=3; s.NumInputs=2; s.DirFeedthrough=1; s.NumSampleTimes=1;
 sys=simsizes(s); x0=[]; str=[]; ts=[0.1 0];
case 3
 Vin=max(u(1),0.1); Vb=max(u(2),10.0);
 D=min(max(Vb/Vin,0.01),0.99);
 Pin=Vin*1.0;
 Pcon=Pin*D*Rds+Pin*(1-D)*Vf/max(Vin,1);
 Psw=(0.5*Coss*Vin^2+Qg*Vgs+0.5*Pin/max(Vin,1)*Vin*tr)*fsw;
 Pdcr=Pin*L_DCR/max(Vin,1);
 Ploss=Pcon+Psw+Pdcr;
 Po=max(0,Pin-Ploss);
 eta_b=Po/max(Pin,0.001);
 Io=Po/max(Vb,10);
 Tj=Tamb+Ploss*Rth;
 sys=[Io; eta_b; Tj];
otherwise, sys=[];
end
