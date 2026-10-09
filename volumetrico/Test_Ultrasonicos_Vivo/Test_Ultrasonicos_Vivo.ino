/* Diagnostico independiente. UNO, monitor serie 115200.
   No mueve servos, no lee/escribe EEPROM, no filtra distancias.
   X TRIG2 ECHO3; Y TRIG4 ECHO5; Z TRIG6 ECHO7. */
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ctype.h>

const byte trigPins[3]={2,4,6}, echoPins[3]={3,5,7};
const float refs[3]={17.5,26,16}; // X altura, Y largo, Z ancho. Solo referencia visual.
LiquidCrystal_I2C panel27(0x27,16,2), panel3f(0x3F,16,2);
LiquidCrystal_I2C *panel=NULL;
struct Stats { unsigned long valid, missing, stuck; float low, high; } stats[3];
float last[3]={0,0,0};
byte status[3]={0,0,0}; // 0 pendiente, 1 eco, 2 timeout, 3 ECHO alto previo
int mode=-1; // -1 todos; 0 X; 1 Y; 2 Z
byte nextAxis=0;
bool running=true;
unsigned long lastPing=0, lastScreen=0;

void resetStats(){
  for(byte i=0;i<3;i++){stats[i].valid=stats[i].missing=stats[i].stuck=0;stats[i].low=10000;stats[i].high=0;status[i]=0;}
  Serial.println(F("# Estadisticas reiniciadas"));
}
void help(){
  Serial.println(F("# TEST ULTRASONICOS / 115200 baudios"));
  Serial.println(F("# X=altura ref17.5 Y=largo ref26 Z=ancho ref16 cm"));
  Serial.println(F("# T=todos X/Y/Z=solo un sensor P=pausa C=continuar R=reiniciar estadisticas ?=ayuda"));
  Serial.println(F("# Teclas individuales; no requieren fin de linea. Sin mediana ni limites de calibracion."));
  Serial.println(F("# ms,eje,eco_us,distancia_cm,estado,min_cm,max_cm,ecos,timeouts,echo_alto"));
}
void showValue(byte i){
  if(status[i]==0)panel->print(F("---"));
  else if(status[i]==2)panel->print(F("SIN"));
  else if(status[i]==3)panel->print(F("ALTO"));
  else panel->print(last[i],1);
}
void updateLCD(){
  if(!panel||millis()-lastScreen<400)return;lastScreen=millis();panel->clear();
  if(mode>=0){panel->print((char)('X'+mode));panel->print(':');showValue(mode);panel->print(F(" cm"));panel->setCursor(0,1);}
  else{panel->print(F("X:"));showValue(0);panel->setCursor(8,0);panel->print(F("Y:"));showValue(1);panel->setCursor(0,1);panel->print(F("Z:"));showValue(2);panel->setCursor(8,1);}
  panel->print(running?F("VIVO"):F("PAUSA"));
}
void measure(byte i){
  unsigned long us=0;
  if(digitalRead(echoPins[i])==HIGH){status[i]=3;stats[i].stuck++;}
  else{
    digitalWrite(trigPins[i],LOW);delayMicroseconds(2);
    digitalWrite(trigPins[i],HIGH);delayMicroseconds(10);digitalWrite(trigPins[i],LOW);
    us=pulseIn(echoPins[i],HIGH,30000UL);
    if(!us){status[i]=2;stats[i].missing++;}
    else{
      status[i]=1;last[i]=us/58.0;stats[i].valid++;
      if(last[i]<stats[i].low)stats[i].low=last[i];
      if(last[i]>stats[i].high)stats[i].high=last[i];
    }
  }
  Serial.print(millis());Serial.print(',');Serial.print((char)('X'+i));Serial.print(',');Serial.print(us);Serial.print(',');
  if(status[i]==1)Serial.print(last[i],2);else Serial.print(F("NA"));
  Serial.print(',');
  if(status[i]==2)Serial.print(F("SIN_ECO_30ms"));
  else if(status[i]==3)Serial.print(F("ECHO_ALTO_ANTES"));
  else if(last[i]<2||last[i]>400)Serial.print(F("FUERA_RANGO_NOMINAL"));
  else if(last[i]>refs[i]+0.5)Serial.print(F("MAS_ALLA_REFERENCIA"));
  else Serial.print(F("ECO"));
  Serial.print(',');if(stats[i].valid)Serial.print(stats[i].low,2);else Serial.print(F("NA"));
  Serial.print(',');if(stats[i].valid)Serial.print(stats[i].high,2);else Serial.print(F("NA"));
  Serial.print(',');Serial.print(stats[i].valid);Serial.print(',');Serial.print(stats[i].missing);Serial.print(',');Serial.println(stats[i].stuck);
}
void setup(){
  Serial.begin(115200);
  // Salidas de control de servos sin pulsos; no se usa Servo.h.
  pinMode(8,OUTPUT);digitalWrite(8,LOW);pinMode(9,OUTPUT);digitalWrite(9,LOW);
  for(byte i=0;i<3;i++){pinMode(trigPins[i],OUTPUT);digitalWrite(trigPins[i],LOW);pinMode(echoPins[i],INPUT);}
  Wire.begin();Wire.setWireTimeout(3000,true);
  Wire.beginTransmission(0x27);if(Wire.endTransmission()==0)panel=&panel27;
  if(!panel){Wire.beginTransmission(0x3F);if(Wire.endTransmission()==0)panel=&panel3f;}
  if(panel){panel->init();panel->backlight();}else Serial.println(F("# LCD no detectado; monitor serie disponible"));
  help();resetStats();
}
void loop(){
  byte budget=16;
  while(Serial.available()&&budget--){char c=toupper((unsigned char)Serial.read());
    if(c=='T'||c=='X'||c=='Y'||c=='Z'){
      mode=c=='T'?-1:c-'X';nextAxis=0;resetStats();Serial.print(F("# Modo "));Serial.println(c);
    }else if(c=='P'){running=false;Serial.println(F("# Pausa"));}
    else if(c=='C'){running=true;Serial.println(F("# Continuar"));}
    else if(c=='R')resetStats();else if(c=='?')help();
  }
  if(running&&millis()-lastPing>=100){lastPing=millis();byte i=mode<0?nextAxis:(byte)mode;measure(i);nextAxis=(i+1)%3;}
  updateLCD();
}
