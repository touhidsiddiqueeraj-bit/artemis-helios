%% Helios-Artemis MPPT Simulator  v3
%% Physics models: Markov+OU irradiance, NARX predictor, IEC61215 PV,
%%                 IRFB4110 buck converter, Shepherd/Peukert/Rint battery
%% Stochastic: each run is an independent random July day (Sylhet, BD)
%% Paper reports mean=94.0%, std=0.6% from 30-day Monte Carlo
%% Usage: run this file once — model is built, simulated, and plotted

warning('off','all');
fprintf('\nStep 1: Clean slate...\n');
if bdIsLoaded('HA_MPPT_v3'), close_system('HA_MPPT_v3',0); end
delete('ha_irr_v3.m','ha_lstm_v3.m','ha_pv_v3.m','ha_buck_v3.m',...
       'ha_battery_v3.m','ha_artemis_v3.m');

%% ???????????????????????????????????????????????????????????????????????????
fprintf('Step 2: Writing S-functions...\n');

%% ?? ha_irr_v3 : Markov irradiance (15 s states) + OU flicker ??????????????
f = fopen('ha_irr_v3.m','w');
fprintf(f,'function [sys,x0,str,ts]=ha_irr_v3(t,x,u,flag)\n');
fprintf(f,'%% Sylhet Markov cloud model (15 s) + Ornstein-Uhlenbeck flicker\n');
fprintf(f,'%% (tau=1 s, sigma=25%%) + aerosol attenuation (0.93)\n');
fprintf(f,'%% States: x(1)=cloud_state  x(2)=cloud_timer  x(3)=G_fast\n');
fprintf(f,'T_row1=[0.60 0.30 0.10]; T_row2=[0.20 0.50 0.30]; T_row3=[0.10 0.20 0.70];\n');
fprintf(f,'cf=[0.15 0.55 0.90]; sunrise=5.5; sunset=18.5;\n');
fprintf(f,'tau_f=1.0; dt=0.1; sigma_frac=0.25; aerosol=0.93; decay=exp(-dt/tau_f);\n');
fprintf(f,'switch flag\n');
fprintf(f,'case 0\n');
fprintf(f,' s=simsizes; s.NumContStates=0; s.NumDiscStates=3;\n');
fprintf(f,' s.NumOutputs=1; s.NumInputs=0; s.DirFeedthrough=0; s.NumSampleTimes=1;\n');
fprintf(f,' sys=simsizes(s); x0=[1;0;0]; str=[]; ts=[0.1 0];\n');
fprintf(f,'case 2\n');
fprintf(f,' cs=x(1); ctimer=x(2); G_fast=x(3);\n');
fprintf(f,' t_h=mod(t,86400)/3600;\n');
fprintf(f,' if t_h>=sunrise && t_h<=sunset\n');
fprintf(f,'  ctimer=ctimer+dt;\n');
fprintf(f,'  if ctimer>=15\n');
fprintf(f,'   ctimer=0; r=rand;\n');
fprintf(f,'   if cs==0, row=T_row1; elseif cs==1, row=T_row2; else, row=T_row3; end\n');
fprintf(f,'   if r<row(1), cs=0; elseif r<row(1)+row(2), cs=1; else, cs=2; end\n');
fprintf(f,'  end\n');
fprintf(f,'  frac=(t_h-sunrise)/(sunset-sunrise);\n');
fprintf(f,'  Gclear=800*aerosol*sin(pi*frac); Gcf=Gclear*cf(cs+1);\n');
fprintf(f,'  sigma_f=sigma_frac*max(Gcf,10);\n');
fprintf(f,'  G_fast=G_fast*decay+sigma_f*sqrt(1-decay^2)*randn;\n');
fprintf(f,'  G_fast=max(-0.40*Gclear, min(0.40*Gclear, G_fast));\n');
fprintf(f,' else\n');
fprintf(f,'  ctimer=0; G_fast=0;\n');
fprintf(f,' end\n');
fprintf(f,' sys=[cs; ctimer; G_fast];\n');
fprintf(f,'case 3\n');
fprintf(f,' t_h=mod(t,86400)/3600;\n');
fprintf(f,' if t_h<sunrise || t_h>sunset, sys=0; return; end\n');
fprintf(f,' cs=x(1); G_fast=x(3);\n');
fprintf(f,' frac=(t_h-sunrise)/(sunset-sunrise);\n');
fprintf(f,' Gclear=800*aerosol*sin(pi*frac); Gcf=Gclear*cf(cs+1);\n');
fprintf(f,' sys=min(Gclear, max(0, Gcf+G_fast));\n');
fprintf(f,'otherwise, sys=[];\n');
fprintf(f,'end\n');
fclose(f);
fprintf('  [OK] ha_irr_v3.m\n');

%% ?? ha_lstm_v3 : NARX predictor + stochastic UART hold ????????????????????
f = fopen('ha_lstm_v3.m','w');
fprintf(f,'function [sys,x0,str,ts]=ha_lstm_v3(t,x,u,flag)\n');
fprintf(f,'%% NARX predictor: 4-unit tanh network + stochastic UART transmission hold\n');
fprintf(f,'%% States: x(1..24)=input buffer  x(25)=hold_counter  x(26)=last_prediction\n');
fprintf(f,'W1=[ 0.42 -0.31  0.18  0.65 -0.22;\n');
fprintf(f,'    -0.55  0.44 -0.38  0.21  0.73;\n');
fprintf(f,'     0.33 -0.62  0.51 -0.44  0.28;\n');
fprintf(f,'    -0.18  0.57 -0.29  0.63 -0.41];\n');
fprintf(f,'W2=[ 0.51 -0.38  0.62 -0.27  0.44];\n');
fprintf(f,'b1=[ 0.1; -0.1; 0.05; -0.05];\n');
fprintf(f,'b2= 0.0;\n');
fprintf(f,'switch flag\n');
fprintf(f,'case 0\n');
fprintf(f,' s=simsizes; s.NumContStates=0; s.NumDiscStates=26;\n');
fprintf(f,' s.NumOutputs=1; s.NumInputs=1; s.DirFeedthrough=0; s.NumSampleTimes=1;\n');
fprintf(f,' sys=simsizes(s); x0=zeros(26,1); str=[]; ts=[0.1 0];\n');
fprintf(f,'case 2\n');
fprintf(f,' buf=x(1:24); G=u(1);\n');
fprintf(f,' buf=[buf(2:end); G/1000];\n');
fprintf(f,' hold_ctr=x(25); last_pred=x(26);\n');
fprintf(f,' hold_ctr=hold_ctr-1;\n');
fprintf(f,' if hold_ctr<=0\n');
fprintf(f,'  new_ctr=3+mod(round(abs(sin(t*17.3))*7),8);\n');
fprintf(f,'  hold_ctr=new_ctr;\n');
fprintf(f,'  fb=[buf(end-3:end); last_pred/1000];\n');
fprintf(f,'  h=tanh(W1*fb+b1);\n');
fprintf(f,'  pred=max(0,(W2*[h;1]+b2)*1000);\n');
fprintf(f,'  last_pred=pred;\n');
fprintf(f,' end\n');
fprintf(f,' sys=[buf; hold_ctr; last_pred];\n');
fprintf(f,'case 3\n');
fprintf(f,' sys=max(0,x(26));\n');
fprintf(f,'otherwise, sys=[];\n');
fprintf(f,'end\n');
fclose(f);
fprintf('  [OK] ha_lstm_v3.m\n');

%% ?? ha_pv_v3 : PV panel IEC 61215 with NOCT thermal model ?????????????????
f = fopen('ha_pv_v3.m','w');
fprintf(f,'function [sys,x0,str,ts]=ha_pv_v3(t,x,u,flag)\n');
fprintf(f,'%% 50 Wp mono-Si panel, IEC 61215 one-diode model\n');
fprintf(f,'%% Soiling factor 3%%, NOCT=45 C, fitted exponent k=14.2606\n');
fprintf(f,'%% such that I(Vmp) == Imp exactly at STC\n');
fprintf(f,'Voc0=21.6; Vmp0=17.8; Isc0=3.0*0.97; Imp0=2.81;\n');
fprintf(f,'k=14.2606;\n');
fprintf(f,'bVoc=-3.4e-3; bVmp=-4.1e-3; aIsc=0.5e-3;\n');
fprintf(f,'NOCT=45; Tamb=35; kJ=0.04;\n');
fprintf(f,'switch flag\n');
fprintf(f,'case 0\n');
fprintf(f,' s=simsizes; s.NumContStates=0; s.NumDiscStates=0;\n');
fprintf(f,' s.NumOutputs=4; s.NumInputs=3; s.DirFeedthrough=1; s.NumSampleTimes=1;\n');
fprintf(f,' sys=simsizes(s); x0=[]; str=[]; ts=[0.1 0];\n');
fprintf(f,'case 3\n');
fprintf(f,' G=max(0,u(1)); Vref=u(2); Tj_ext=u(3);\n');
fprintf(f,' g=G/1000;\n');
fprintf(f,' Tc=Tamb+(NOCT-20)/800*G+kJ*max(0,Tj_ext-40);\n');
fprintf(f,' dT=Tc-25;\n');
fprintf(f,' Voc_T=Voc0*(1+bVoc*dT);\n');
fprintf(f,' Vmp_T=Vmp0*(1+bVmp*dT);\n');
fprintf(f,' Isc_T=Isc0*(1+aIsc*dT)*g;\n');
fprintf(f,' Imp_T=Imp0*g;\n');
fprintf(f,' Vr=min(max(Vref,0.1), Voc_T-0.05);\n');
fprintf(f,' Ipv=max(0, Isc_T*(1-(Vr/Voc_T)^k));\n');
fprintf(f,' Ppv=Ipv*Vr;\n');
fprintf(f,' Vmp_c=Voc_T*(1/(1+k))^(1/k);\n');
fprintf(f,' Pm=max(0, Isc_T*(1-(Vmp_c/Voc_T)^k)*Vmp_c);\n');
fprintf(f,' sys=[Ipv; Vr; Ppv; Pm];\n');
fprintf(f,'otherwise, sys=[];\n');
fprintf(f,'end\n');
fclose(f);
fprintf('  [OK] ha_pv_v3.m\n');

%% ?? ha_buck_v3 : IRFB4110 buck converter with dynamic duty and loss model ??
f = fopen('ha_buck_v3.m','w');
fprintf(f,'function [sys,x0,str,ts]=ha_buck_v3(t,x,u,flag)\n');
fprintf(f,'%% IRFB4110 synchronous buck: D=Vbat/Vin, conduction+switching+DCR losses\n');
fprintf(f,'Rds=3.7e-3; L_DCR=50e-3; Vf=0.45;\n');
fprintf(f,'Coss=83e-9; Qg=10e-9; Vgs=10; tr=20e-9; fsw=50e3;\n');
fprintf(f,'Rth=18.55; Tamb=40;\n');
fprintf(f,'switch flag\n');
fprintf(f,'case 0\n');
fprintf(f,' s=simsizes; s.NumContStates=0; s.NumDiscStates=0;\n');
fprintf(f,' s.NumOutputs=3; s.NumInputs=2; s.DirFeedthrough=1; s.NumSampleTimes=1;\n');
fprintf(f,' sys=simsizes(s); x0=[]; str=[]; ts=[0.1 0];\n');
fprintf(f,'case 3\n');
fprintf(f,' Vin=max(u(1),0.1); Vb=max(u(2),10.0);\n');
fprintf(f,' D=min(max(Vb/Vin,0.01),0.99);\n');
fprintf(f,' Pin=Vin*1.0;\n');
fprintf(f,' Pcon=Pin*D*Rds+Pin*(1-D)*Vf/max(Vin,1);\n');
fprintf(f,' Psw=(0.5*Coss*Vin^2+Qg*Vgs+0.5*Pin/max(Vin,1)*Vin*tr)*fsw;\n');
fprintf(f,' Pdcr=Pin*L_DCR/max(Vin,1);\n');
fprintf(f,' Ploss=Pcon+Psw+Pdcr;\n');
fprintf(f,' Po=max(0,Pin-Ploss);\n');
fprintf(f,' eta_b=Po/max(Pin,0.001);\n');
fprintf(f,' Io=Po/max(Vb,10);\n');
fprintf(f,' Tj=Tamb+Ploss*Rth;\n');
fprintf(f,' sys=[Io; eta_b; Tj];\n');
fprintf(f,'otherwise, sys=[];\n');
fprintf(f,'end\n');
fclose(f);
fprintf('  [OK] ha_buck_v3.m\n');

%% ?? ha_battery_v3 : 7 Ah 12 V SLA — Shepherd OCV + Peukert + Rint ?????????
f = fopen('ha_battery_v3.m','w');
fprintf(f,'function [sys,x0,str,ts]=ha_battery_v3(t,x,u,flag)\n');
fprintf(f,'%% 7 Ah / 12 V SLA battery: Shepherd OCV, Peukert capacity loss, Rint=50 mohm\n');
fprintf(f,'%% States: x(1)=charge_Ah  x(2)=terminal_voltage (warm-start avoids t=0 spike)\n');
fprintf(f,'Qnom=7.0; Irate=1.0; np=1.25; Rint=0.05;\n');
fprintf(f,'SoC0=0.30;\n');
fprintf(f,'Voc0_=11.84+1.98*SoC0-0.28*SoC0^2;\n');
fprintf(f,'Vt0_=Voc0_;\n');
fprintf(f,'switch flag\n');
fprintf(f,'case 0\n');
fprintf(f,' s=simsizes; s.NumContStates=0; s.NumDiscStates=2;\n');
fprintf(f,' s.NumOutputs=2; s.NumInputs=1; s.DirFeedthrough=0; s.NumSampleTimes=1;\n');
fprintf(f,' sys=simsizes(s); x0=[SoC0*Qnom; Vt0_]; str=[]; ts=[0.1 0];\n');
fprintf(f,'case 2\n');
fprintf(f,' Q=x(1); Ic=max(0,u(1));\n');
fprintf(f,' dQ=(Ic/max(Irate,0.01))^(np-1)*Ic*0.1/3600;\n');
fprintf(f,' Q_new=min(Q+dQ, Qnom);\n');
fprintf(f,' SoC_new=Q_new/Qnom;\n');
fprintf(f,' Voc_new=11.84+1.98*SoC_new-0.28*SoC_new^2;\n');
fprintf(f,' Vt_new=Voc_new+Rint*Ic;\n');
fprintf(f,' sys=[Q_new; Vt_new];\n');
fprintf(f,'case 3\n');
fprintf(f,' SoC=x(1)/Qnom;\n');
fprintf(f,' sys=[x(2); SoC];\n');
fprintf(f,'otherwise, sys=[];\n');
fprintf(f,'end\n');
fclose(f);
fprintf('  [OK] ha_battery_v3.m\n');

%% ?? ha_artemis_v3 : VS-P&O + LSTM predictive blend ????????????????????????
f = fopen('ha_artemis_v3.m','w');
fprintf(f,'function [sys,x0,str,ts]=ha_artemis_v3(t,x,u,flag)\n');
fprintf(f,'%% Artemis MPPT: variable-step P&O blended with NARX irradiance prediction\n');
fprintf(f,'%% ADC noise: sigma_I=2 mA, sigma_V=10 mV\n');
fprintf(f,'%% States: x(1)=Vref  x(2)=P_prev  x(3)=V_prev  x(4)=cooldown_counter\n');
fprintf(f,'alpha_base=0.45; Vmp_nom=17.8; Voc_nom=21.6;\n');
fprintf(f,'sigma_I=0.002; sigma_V=0.010;\n');
fprintf(f,'switch flag\n');
fprintf(f,'case 0\n');
fprintf(f,' s=simsizes; s.NumContStates=0; s.NumDiscStates=4;\n');
fprintf(f,' s.NumOutputs=3; s.NumInputs=6; s.DirFeedthrough=1; s.NumSampleTimes=1;\n');
fprintf(f,' sys=simsizes(s); x0=[Vmp_nom;0;Vmp_nom;0]; str=[]; ts=[0.1 0];\n');
fprintf(f,'case 2\n');
fprintf(f,' Vr=x(1); Pp=x(2); Vp_prev=x(3); cool=x(4);\n');
fprintf(f,' Im=max(0,u(1)+sigma_I*randn); Vm=max(0.1,u(2)+sigma_V*randn);\n');
fprintf(f,' P=Im*Vm; dP=P-Pp; dV=Vm-Vp_prev;\n');
fprintf(f,' dl=min(max(0.008*abs(dP/(abs(dV)+1e-9)),0.05),0.60);\n');
fprintf(f,' if dP>=0; if dV>=0, Vr=Vr+dl; else, Vr=Vr-dl; end\n');
fprintf(f,' else; if dV>=0, Vr=Vr-dl; else, Vr=Vr+dl; end; end\n');
fprintf(f,' Gp=u(3); Gn=u(4); cool=max(0,cool-1);\n');
fprintf(f,' if cool==0 && Gn>80 && Gp>80\n');
fprintf(f,'  rel_dev=abs(Gp-Gn)/max(max(Gp,Gn),10);\n');
fprintf(f,'  if rel_dev>0.15\n');
fprintf(f,'   if Gp>Gn, alpha=alpha_base*exp(-1.5*rel_dev);\n');
fprintf(f,'   else, alpha=0.08*exp(-1.0*rel_dev); end\n');
fprintf(f,'   Vmppt=Vmp_nom*(1-0.014*log(max(Gp,1)/1000));\n');
fprintf(f,'   Vr=(1-alpha)*Vr+alpha*Vmppt; cool=20;\n');
fprintf(f,'  end\n');
fprintf(f,' end\n');
fprintf(f,' Vr=min(max(Vr, 0.60*Voc_nom), 0.98*Voc_nom);\n');
fprintf(f,' sys=[Vr; P; Vm; cool];\n');
fprintf(f,'case 3\n');
fprintf(f,' Pm=max(u(6),0.01); Pcurr=max(0,u(1)*u(2));\n');
fprintf(f,' eta=min(1.0, Pcurr/Pm);\n');
fprintf(f,' if u(5)<14.70, cs=1; elseif u(5)<14.76, cs=2; else, cs=3; end\n');
fprintf(f,' sys=[x(1); eta; cs];\n');
fprintf(f,'otherwise, sys=[];\n');
fprintf(f,'end\n');
fclose(f);
fprintf('  [OK] ha_artemis_v3.m\n');

%% ???????????????????????????????????????????????????????????????????????????
fprintf('Step 3: Building Simulink model...\n');
mdl = 'HA_MPPT_v3';
new_system(mdl);
set_param(mdl,'Solver','FixedStepDiscrete','FixedStep','0.1',...
    'StartTime','0','StopTime','86400',...
    'SignalLogging','off');

%% ?? Irradiance ?????????????????????????????????????????????????????????????
add_block('built-in/S-Function',[mdl '/Irr'],...
    'Position',[50 50 180 80],'FunctionName','ha_irr_v3','Parameters','');

%% ?? NARX predictor ?????????????????????????????????????????????????????????
add_block('built-in/S-Function',[mdl '/LSTM'],...
    'Position',[50 110 180 140],'FunctionName','ha_lstm_v3','Parameters','');
add_line(mdl,'Irr/1','LSTM/1','autorouting','on');

%% ?? Unit delays (break algebraic loops) ????????????????????????????????????
add_block('built-in/Unit Delay',[mdl '/VrefDly'],...
    'Position',[50 170 130 200],'InitialCondition','17.8','SampleTime','0.1');
add_block('built-in/Unit Delay',[mdl '/TjDly'],...
    'Position',[50 220 130 250],'InitialCondition','40','SampleTime','0.1');

%% ?? PV Panel subsystem ?????????????????????????????????????????????????????
blk = [mdl '/PVPanel'];
add_block('built-in/SubSystem',blk,'Position',[220 40 390 230]);
add_block('built-in/Inport', [blk '/G'],  'Position',[20 40  50 60], 'Port','1');
add_block('built-in/Inport', [blk '/Vr'], 'Position',[20 100 50 120],'Port','2');
add_block('built-in/Inport', [blk '/Tj'], 'Position',[20 160 50 180],'Port','3');
add_block('built-in/Outport',[blk '/Ip'], 'Position',[300 40  330 60], 'Port','1');
add_block('built-in/Outport',[blk '/Vp'], 'Position',[300 90  330 110],'Port','2');
add_block('built-in/Outport',[blk '/Pp'], 'Position',[300 140 330 160],'Port','3');
add_block('built-in/Outport',[blk '/Pm'], 'Position',[300 190 330 210],'Port','4');
add_block('built-in/Mux',    [blk '/mx'], 'Position',[80 40 100 200],'Inputs','3');
add_block('built-in/S-Function',[blk '/sf'],'Position',[130 40 270 200],...
    'FunctionName','ha_pv_v3','Parameters','');
add_block('built-in/Demux',[blk '/dm'],'Position',[280 40 290 210],'Outputs','4');
add_line(blk,'G/1', 'mx/1','autorouting','on');
add_line(blk,'Vr/1','mx/2','autorouting','on');
add_line(blk,'Tj/1','mx/3','autorouting','on');
add_line(blk,'mx/1','sf/1','autorouting','on');
add_line(blk,'sf/1','dm/1','autorouting','on');
add_line(blk,'dm/1','Ip/1','autorouting','on');
add_line(blk,'dm/2','Vp/1','autorouting','on');
add_line(blk,'dm/3','Pp/1','autorouting','on');
add_line(blk,'dm/4','Pm/1','autorouting','on');

%% ?? Artemis MPPT subsystem ?????????????????????????????????????????????????
blk = [mdl '/ArtemisMPPT'];
add_block('built-in/SubSystem',blk,'Position',[420 40 590 320]);
add_block('built-in/Inport',[blk '/Ip'],'Position',[20 40  50 60], 'Port','1');
add_block('built-in/Inport',[blk '/Vp'],'Position',[20 100 50 120],'Port','2');
add_block('built-in/Inport',[blk '/Gp'],'Position',[20 160 50 180],'Port','3');
add_block('built-in/Inport',[blk '/Gn'],'Position',[20 220 50 240],'Port','4');
add_block('built-in/Inport',[blk '/Vb'],'Position',[20 280 50 300],'Port','5');
add_block('built-in/Inport',[blk '/Pm'],'Position',[20 340 50 360],'Port','6');
add_block('built-in/Outport',[blk '/Vr'],'Position',[340 40  370 60], 'Port','1');
add_block('built-in/Outport',[blk '/et'],'Position',[340 90  370 110],'Port','2');
add_block('built-in/Outport',[blk '/cs'],'Position',[340 140 370 160],'Port','3');
add_block('built-in/Mux',[blk '/mx'],'Position',[80 40 100 380],'Inputs','6');
add_block('built-in/S-Function',[blk '/sf'],'Position',[130 40 290 380],...
    'FunctionName','ha_artemis_v3','Parameters','');
add_block('built-in/Demux',[blk '/dm'],'Position',[310 40 320 170],'Outputs','3');
add_line(blk,'Ip/1','mx/1','autorouting','on');
add_line(blk,'Vp/1','mx/2','autorouting','on');
add_line(blk,'Gp/1','mx/3','autorouting','on');
add_line(blk,'Gn/1','mx/4','autorouting','on');
add_line(blk,'Vb/1','mx/5','autorouting','on');
add_line(blk,'Pm/1','mx/6','autorouting','on');
add_line(blk,'mx/1','sf/1','autorouting','on');
add_line(blk,'sf/1','dm/1','autorouting','on');
add_line(blk,'dm/1','Vr/1','autorouting','on');
add_line(blk,'dm/2','et/1','autorouting','on');
add_line(blk,'dm/3','cs/1','autorouting','on');

%% ?? Buck converter subsystem ???????????????????????????????????????????????
blk = [mdl '/BuckConv'];
add_block('built-in/SubSystem',blk,'Position',[420 340 590 440]);
add_block('built-in/Inport', [blk '/Vr'], 'Position',[20 40 50 60],'Port','1');
add_block('built-in/Inport', [blk '/Vb'], 'Position',[20 100 50 120],'Port','2');
add_block('built-in/Outport',[blk '/Io'], 'Position',[200 40  230 60], 'Port','1');
add_block('built-in/Outport',[blk '/eta'],'Position',[200 90  230 110],'Port','2');
add_block('built-in/Outport',[blk '/Tj'], 'Position',[200 140 230 160],'Port','3');
add_block('built-in/Mux',[blk '/mx'],'Position',[80 40 100 130],'Inputs','2');
add_block('built-in/S-Function',[blk '/sf'],'Position',[130 40 180 160],...
    'FunctionName','ha_buck_v3','Parameters','');
add_block('built-in/Demux',[blk '/dm'],'Position',[185 40 195 170],'Outputs','3');
add_line(blk,'Vr/1','mx/1','autorouting','on');
add_line(blk,'Vb/1','mx/2','autorouting','on');
add_line(blk,'mx/1','sf/1','autorouting','on');
add_line(blk,'sf/1','dm/1','autorouting','on');
add_line(blk,'dm/1','Io/1','autorouting','on');
add_line(blk,'dm/2','eta/1','autorouting','on');
add_line(blk,'dm/3','Tj/1','autorouting','on');

%% ?? Battery subsystem ??????????????????????????????????????????????????????
blk = [mdl '/Battery'];
add_block('built-in/SubSystem',blk,'Position',[620 340 790 440]);
add_block('built-in/Inport', [blk '/Ic'], 'Position',[20 70 50 90],'Port','1');
add_block('built-in/Outport',[blk '/Vt'], 'Position',[200 40 230 60],'Port','1');
add_block('built-in/Outport',[blk '/SoC'],'Position',[200 90 230 110],'Port','2');
add_block('built-in/S-Function',[blk '/sf'],'Position',[80 40 170 120],...
    'FunctionName','ha_battery_v3','Parameters','');
add_block('built-in/Demux',[blk '/dm'],'Position',[175 40 185 120],'Outputs','2');
add_line(blk,'Ic/1','sf/1','autorouting','on');
add_line(blk,'sf/1','dm/1','autorouting','on');
add_line(blk,'dm/1','Vt/1','autorouting','on');
add_line(blk,'dm/2','SoC/1','autorouting','on');

%% ?? Workspace loggers ??????????????????????????????????????????????????????
loggers = {'log_ghi','log_ppv','log_pmpp','log_eta','log_soc','log_vbat','log_tj'};
ypos    = 50:50:400;
for i = 1:numel(loggers)
    add_block('built-in/To Workspace',[mdl '/' loggers{i}],...
        'Position',[800 ypos(i) 900 ypos(i)+20],...
        'VariableName',loggers{i},'SaveFormat','Array','MaxDataPoints','inf');
end

%% ?? Terminators ????????????????????????????????????????????????????????????
add_block('built-in/Terminator',[mdl '/TrmSt'],  'Position',[800 400 820 420]);
add_block('built-in/Terminator',[mdl '/TrmBeta'],'Position',[800 430 820 450]);

%% ?? Top-level wiring ???????????????????????????????????????????????????????
add_line(mdl,'Irr/1',        'PVPanel/1',     'autorouting','on');
add_line(mdl,'VrefDly/1',    'PVPanel/2',     'autorouting','on');
add_line(mdl,'TjDly/1',      'PVPanel/3',     'autorouting','on');
add_line(mdl,'PVPanel/1',    'ArtemisMPPT/1', 'autorouting','on');
add_line(mdl,'PVPanel/2',    'ArtemisMPPT/2', 'autorouting','on');
add_line(mdl,'LSTM/1',       'ArtemisMPPT/3', 'autorouting','on');
add_line(mdl,'Irr/1',        'ArtemisMPPT/4', 'autorouting','on');
add_line(mdl,'PVPanel/4',    'ArtemisMPPT/6', 'autorouting','on');
add_line(mdl,'ArtemisMPPT/1','VrefDly/1',     'autorouting','on');
add_line(mdl,'ArtemisMPPT/1','BuckConv/1',    'autorouting','on');
add_line(mdl,'Battery/1',    'ArtemisMPPT/5', 'autorouting','on');
add_line(mdl,'Battery/1',    'BuckConv/2',    'autorouting','on');
add_line(mdl,'BuckConv/1',   'Battery/1',     'autorouting','on');
add_line(mdl,'BuckConv/3',   'TjDly/1',       'autorouting','on');
add_line(mdl,'BuckConv/2',   'TrmBeta/1',     'autorouting','on');
add_line(mdl,'ArtemisMPPT/3','TrmSt/1',       'autorouting','on');
add_line(mdl,'Irr/1',        'log_ghi/1',     'autorouting','on');
add_line(mdl,'PVPanel/3',    'log_ppv/1',     'autorouting','on');
add_line(mdl,'PVPanel/4',    'log_pmpp/1',    'autorouting','on');
add_line(mdl,'ArtemisMPPT/2','log_eta/1',     'autorouting','on');
add_line(mdl,'Battery/2',    'log_soc/1',     'autorouting','on');
add_line(mdl,'Battery/1',    'log_vbat/1',    'autorouting','on');
add_line(mdl,'BuckConv/3',   'log_tj/1',      'autorouting','on');

%% ?? Save and run ???????????????????????????????????????????????????????????
save_system(mdl);
fprintf('  [OK] HA_MPPT_v3.slx saved\n');
addpath(pwd); rehash;

fprintf('Step 4: Running simulation (86400 s)...\n');
sim('HA_MPPT_v3');
fprintf('  [OK] Simulation complete\n');

%% ???????????????????????????????????????????????????????????????????????????
%% POST-SIMULATION ANALYSIS AND PLOTTING
%% ???????????????????????????????????????????????????????????????????????????

%% ?? Time axis ??????????????????????????????????????????????????????????????
dt  = 0.1;
N   = length(log_ghi);
t_s = (0:N-1)' * dt;
t_h = t_s / 3600;

%% ?? Signal conditioning ????????????????????????????????????????????????????
log_ppv  = max(log_ppv,  0);
log_pmpp = max(log_pmpp, 0);
log_eta  = max(log_eta,  0);
log_ghi  = max(log_ghi,  0);

%% ?? Derived metrics ????????????????????????????????????????????????????????
mask     = log_pmpp > 0.5;
eta_inst = zeros(N,1);
eta_inst(mask) = min(log_ppv(mask) ./ log_pmpp(mask), 1.0);

E_pv    = trapz(t_s, log_ppv)  / 3600;
E_mpp   = trapz(t_s, log_pmpp) / 3600;
eta_day = E_pv / max(E_mpp, 1) * 100;

G_peak  = max(log_ghi);
P_peak  = max(log_ppv);
SoC_end = log_soc(end) * 100;
Vb_end  = log_vbat(end);
Tj_peak = max(log_tj);
E_lost  = max(E_mpp - E_pv, 0);

%% ?? Console summary ????????????????????????????????????????????????????????
fprintf('\n==========================================\n');
fprintf('  DAILY SUMMARY\n');
fprintf('==========================================\n');
fprintf('  Peak GHI          : %6.1f  W/m2\n',  G_peak);
fprintf('  Peak PV power     : %6.2f  W\n',     P_peak);
fprintf('  Energy harvested  : %6.2f  Wh\n',    E_pv);
fprintf('  MPP energy avail  : %6.2f  Wh\n',    E_mpp);
fprintf('  Daily MPPT eta    : %6.2f  %%\n',    eta_day);
fprintf('  Battery SoC (end) : %6.1f  %%\n',    SoC_end);
fprintf('  Battery Vt  (end) : %6.3f  V\n',     Vb_end);
fprintf('  MOSFET Tj peak    : %6.1f  degC\n',  Tj_peak);
fprintf('==========================================\n\n');

%% ?? Rolling 30-min efficiency (used in Fig 3) ?????????????????????????????
win      = round(1800 / dt);
eta_roll = movmean(eta_inst, win);

%% ???????????????????????????????????????????????????????????????????????????
%% Figure 1 — Irradiance & PV Power
%% ???????????????????????????????????????????????????????????????????????????
figure('Name','HA-MPPT v3 — Irradiance & Power','Color','w',...
       'Position',[60 60 1120 720]);

ax1 = subplot(3,1,1);
plot(t_h, log_ghi,'Color',[0.85 0.55 0.10],'LineWidth',1.2);
ylabel('GHI  (W/m^2)');
title('Irradiance — Sylhet Markov + Ornstein-Uhlenbeck model');
xlim([0 24]); grid on; grid minor;
set(ax1,'XTick',0:2:24,'FontSize',10);
xlabel('Time of day  (h)');

ax2 = subplot(3,1,2);
hold on;
area(t_h, log_pmpp,'FaceColor',[0.75 0.88 1.0],'EdgeColor','none');
plot(t_h, log_ppv,'b','LineWidth',1.4);
hold off;
ylabel('Power  (W)');
title('PV Output vs MPP Available');
legend('P_{MPP} available','P_{PV} harvested','Location','northwest');
xlim([0 24]); grid on; grid minor;
set(ax2,'XTick',0:2:24,'FontSize',10);
xlabel('Time of day  (h)');

ax3 = subplot(3,1,3);
plot(t_h(mask), eta_inst(mask)*100,'Color',[0.10 0.60 0.20],'LineWidth',1.0);
hold on;
plot([0 24],[eta_day eta_day],'r--','LineWidth',1.5);
text(0.5, eta_day+0.8, sprintf('Daily \\eta = %.2f%%', eta_day),...
     'Color','r','FontSize',9,'FontWeight','bold');
hold off;
ylabel('MPPT \eta  (%)');
title('Instantaneous MPPT Efficiency');
ylim([50 105]); xlim([0 24]); grid on; grid minor;
set(ax3,'XTick',0:2:24,'FontSize',10);
xlabel('Time of day  (h)');

sgtitle('Helios-Artemis MPPT v3  —  Single-Day Simulation',...
        'FontSize',13,'FontWeight','bold');

%% ???????????????????????????????????????????????????????????????????????????
%% Figure 2 — Battery & Thermal
%% ???????????????????????????????????????????????????????????????????????????
figure('Name','HA-MPPT v3 — Battery & Thermal','Color','w',...
       'Position',[100 100 1120 620]);

ax4 = subplot(2,2,1);
plot(t_h, log_soc*100,'Color',[0.15 0.45 0.80],'LineWidth',1.4);
ylabel('State of Charge  (%)');
title('Battery SoC');
ylim([0 105]); xlim([0 24]); grid on; grid minor;
set(ax4,'XTick',0:2:24,'FontSize',10);
xlabel('Time of day  (h)');

ax5 = subplot(2,2,2);
plot(t_h, log_vbat,'Color',[0.60 0.10 0.70],'LineWidth',1.4);
ylabel('Terminal Voltage  (V)');
title('Battery Terminal Voltage');
xlim([0 24]); grid on; grid minor;
set(ax5,'XTick',0:2:24,'FontSize',10);
xlabel('Time of day  (h)');

ax6 = subplot(2,2,3);
plot(t_h, log_tj,'Color',[0.80 0.15 0.15],'LineWidth',1.4);
hold on;
plot([0 24],[150 150],'k--','LineWidth',1.2);
text(0.5, 152, 'T_j limit 150°C','FontSize',8,'Color','k');
hold off;
ylabel('Junction Temp  (°C)');
title('MOSFET Junction Temperature  (IRFB4110)');
xlim([0 24]); grid on; grid minor;
set(ax6,'XTick',0:2:24,'FontSize',10);
xlabel('Time of day  (h)');

ax7 = subplot(2,2,4);
scatter(log_ghi(mask), eta_inst(mask)*100, 2,[0.20 0.60 0.80],...
        'filled','MarkerFaceAlpha',0.3);
xlabel('GHI  (W/m^2)');
ylabel('MPPT \eta  (%)');
title('Efficiency vs Irradiance  (scatter)');
ylim([50 105]); grid on; grid minor;
set(ax7,'FontSize',10);

sgtitle('Helios-Artemis MPPT v3  —  Battery & Thermal',...
        'FontSize',13,'FontWeight','bold');

%% ???????????????????????????????????????????????????????????????????????????
%% Figure 3 — Energy Summary & 30-min Rolling Efficiency
%% ???????????????????????????????????????????????????????????????????????????
figure('Name','HA-MPPT v3 — Energy Summary','Color','w',...
       'Position',[140 140 1020 460]);

subplot(1,2,1);
pie([E_pv, E_lost],...
    {sprintf('Harvested\n%.2f Wh (%.1f%%)', E_pv,  eta_day),...
     sprintf('Lost\n%.2f Wh (%.1f%%)',       E_lost, 100-eta_day)});
colormap(gca,[0.20 0.65 0.30; 0.85 0.25 0.20]);
title('Daily Energy Budget','FontSize',11);

ax8 = subplot(1,2,2);
plot(t_h(mask), eta_roll(mask)*100,'Color',[0.10 0.50 0.30],'LineWidth',1.6);
hold on;
plot([0 24],[eta_day eta_day],'r--','LineWidth',1.4);
text(0.5, eta_day+0.8, sprintf('Daily \\eta = %.2f%%', eta_day),...
     'Color','r','FontSize',9,'FontWeight','bold');
hold off;
ylabel('30-min Rolling \eta  (%)');
xlabel('Time of day  (h)');
title('30-Minute Rolling MPPT Efficiency');
ylim([50 105]); xlim([0 24]); grid on; grid minor;
set(ax8,'XTick',0:2:24,'FontSize',10);

sgtitle('Helios-Artemis MPPT v3  —  Energy Summary',...
        'FontSize',13,'FontWeight','bold');

%% ???????????????????????????????????????????????????????????????????????????
%% Figure 4 — Daily Summary Card
%% ???????????????????????????????????????????????????????????????????????????
figure('Name','HA-MPPT v3 — Daily Summary','Color','w',...
       'Position',[200 200 560 420]);
axis off;

% Panel background
annotation('rectangle',[0.05 0.08 0.90 0.86],...
           'FaceColor',[0.97 0.97 0.97],'EdgeColor',[0.70 0.70 0.70],...
           'LineWidth',1.5);

% Title bar
annotation('rectangle',[0.05 0.82 0.90 0.12],...
           'FaceColor',[0.15 0.35 0.60],'EdgeColor','none');
annotation('textbox',[0.05 0.82 0.90 0.12],'String','DAILY SUMMARY',...
           'Color','w','FontSize',14,'FontWeight','bold',...
           'HorizontalAlignment','center','VerticalAlignment','middle',...
           'EdgeColor','none');

% Metric rows  [label, value, unit, row_index]
metrics = {
    'Peak GHI',         sprintf('%.1f',  G_peak),   'W/m^2',  1;
    'Peak PV Power',    sprintf('%.2f',  P_peak),   'W',      2;
    'Energy Harvested', sprintf('%.2f',  E_pv),     'Wh',     3;
    'MPP Energy Avail', sprintf('%.2f',  E_mpp),    'Wh',     4;
    'Daily MPPT \eta',  sprintf('%.2f',  eta_day),  '%',      5;
    'Battery SoC (end)',sprintf('%.1f',  SoC_end),  '%',      6;
    'Battery Vt (end)', sprintf('%.3f',  Vb_end),   'V',      7;
    'MOSFET Tj Peak',   sprintf('%.1f',  Tj_peak),  '^oC',    8;
};

nRows  = size(metrics,1);
rowH   = 0.72 / nRows;   % total height allocated for rows
yBase  = 0.80;           % top of first row (just below title bar)

% Column colours (alternating)
rowColors = {[1 1 1],[0.94 0.96 1.0]};

for i = 1:nRows
    yTop = yBase - i*rowH;
    % Alternating row background
    annotation('rectangle',[0.06 yTop 0.88 rowH-0.005],...
               'FaceColor',rowColors{mod(i,2)+1},'EdgeColor','none');
    % Label (left)
    annotation('textbox',[0.08 yTop 0.52 rowH],...
               'String',metrics{i,1},...
               'FontSize',10,'FontWeight','normal',...
               'HorizontalAlignment','left','VerticalAlignment','middle',...
               'EdgeColor','none','BackgroundColor','none');
    % Value (right, bold)
    valStr = [metrics{i,2} '  ' metrics{i,3}];
    annotation('textbox',[0.55 yTop 0.40 rowH],...
               'String',valStr,...
               'FontSize',10,'FontWeight','bold','Color',[0.10 0.30 0.60],...
               'HorizontalAlignment','right','VerticalAlignment','middle',...
               'EdgeColor','none','BackgroundColor','none');
end

% Divider line between label and value columns
annotation('line',[0.60 0.60],[0.09 0.82],...
           'Color',[0.75 0.75 0.75],'LineWidth',0.8);

title('','Visible','off');

fprintf('Plots complete.\n');