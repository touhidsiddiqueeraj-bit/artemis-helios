function [sys,x0,str,ts]=ha_lstm_v3(t,x,u,flag)
% NARX predictor: 4-unit tanh network + stochastic UART transmission hold
% States: x(1..24)=input buffer  x(25)=hold_counter  x(26)=last_prediction
W1=[ 0.42 -0.31  0.18  0.65 -0.22;
    -0.55  0.44 -0.38  0.21  0.73;
     0.33 -0.62  0.51 -0.44  0.28;
    -0.18  0.57 -0.29  0.63 -0.41];
W2=[ 0.51 -0.38  0.62 -0.27  0.44];
b1=[ 0.1; -0.1; 0.05; -0.05];
b2= 0.0;
switch flag
case 0
 s=simsizes; s.NumContStates=0; s.NumDiscStates=26;
 s.NumOutputs=1; s.NumInputs=1; s.DirFeedthrough=0; s.NumSampleTimes=1;
 sys=simsizes(s); x0=zeros(26,1); str=[]; ts=[0.1 0];
case 2
 buf=x(1:24); G=u(1);
 buf=[buf(2:end); G/1000];
 hold_ctr=x(25); last_pred=x(26);
 hold_ctr=hold_ctr-1;
 if hold_ctr<=0
  new_ctr=3+mod(round(abs(sin(t*17.3))*7),8);
  hold_ctr=new_ctr;
  fb=[buf(end-3:end); last_pred/1000];
  h=tanh(W1*fb+b1);
  pred=max(0,(W2*[h;1]+b2)*1000);
  last_pred=pred;
 end
 sys=[buf; hold_ctr; last_pred];
case 3
 sys=max(0,x(26));
otherwise, sys=[];
end
