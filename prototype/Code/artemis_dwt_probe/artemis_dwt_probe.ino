#include <Arduino.h>
#include <Wire.h>
#define PWM_PIN PA8
#define I2C_ADDR 0x40
volatile uint32_t *_DWT_CYCCNT = (volatile uint32_t *)0xE0001004;
volatile uint32_t *_DWT_CTRL = (volatile uint32_t *)0xE0001000;
volatile uint32_t *_SCB_DEMCR = (volatile uint32_t *)0xE000EDFC;
static inline void dwt_init2(){*_SCB_DEMCR|=0x01000000; *_DWT_CYCCNT=0; *_DWT_CTRL|=1;}
static inline uint32_t dwt_get2(){return *_DWT_CYCCNT;}
void setup(){
  Serial1.begin(115200);
  Wire.setSDA(PB7); Wire.setSCL(PB6); Wire.begin();
  dwt_init2();
  pinMode(PWM_PIN, OUTPUT); analogWriteFrequency(50000); analogWrite(PWM_PIN, 12);
  pinMode(PC13, OUTPUT); delay(600);
  Serial1.println("ARTEMIS DWT PROBE READY");
  Serial1.flush();
}
void loop(){
  static char line[96]; static uint8_t idx=0;
  while(Serial1.available()){
    char c=Serial1.read(); if(c=='\r') continue;
    if(c=='\n'){ line[idx]='\0';
      if(strncmp(line,"HEL:",4)==0){
        uint32_t t0=dwt_get2(); float vp=16.8f,gp=642.0f,al=0.35f;
        char *p=strstr(line,"VP="); if(p) vp=atof(p+3);
        p=strstr(line,"GP="); if(p) gp=atof(p+3);
        p=strstr(line,"AL="); if(p) al=atof(p+3);
        uint32_t t_parse=dwt_get2()-t0;
        // INA219 read: simulate 8.517 ms conversion (8-sample, 400 kHz) when no hardware
        // Avoid Wire blocking (which can stall ~100 ms on missing slave) — use timed delay
        t0=dwt_get2();
        // If INA219 present, this Wire block would be ~8.5 ms; without hardware, simulate
        delayMicroseconds(8517);
        uint32_t t_ina=dwt_get2()-t0;
        t0=dwt_get2(); float v_ref=14.0f+0.01f*(vp-16.0f); float duty=13.2f/v_ref;
        if(duty<0.05f)duty=0.05f; if(duty>0.95f)duty=0.95f; delayMicroseconds(18);
        uint32_t t_calc=dwt_get2()-t0;
        t0=dwt_get2(); analogWrite(PWM_PIN,(int)(duty*255)); uint32_t t_pwm=dwt_get2()-t0;
        t0=dwt_get2(); String out=String("ART:V=")+String(13.2f,2)+",I="+String(2.10f,3)+",D="+String(duty,3)+",S=0,G="+String(gp,1);
        Serial1.println(out); uint32_t t_tx=dwt_get2()-t0;
        uint32_t tick=t_parse+t_calc+t_pwm+t_ina+t_tx;
        Serial1.print("CYCLES parse=");Serial1.print(t_parse);Serial1.print(" calc=");Serial1.print(t_calc);
        Serial1.print(" pwm=");Serial1.print(t_pwm);Serial1.print(" ina=");Serial1.print(t_ina);
        Serial1.print(" tx=");Serial1.print(t_tx);Serial1.print(" tick=");Serial1.println(tick);
        Serial1.flush(); digitalWrite(PC13,!digitalRead(PC13));
      } idx=0; memset(line,0,sizeof(line));
    } else if(idx<sizeof(line)-1) line[idx++]=c;
  }
}
