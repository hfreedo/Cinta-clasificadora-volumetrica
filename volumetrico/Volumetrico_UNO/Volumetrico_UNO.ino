/* CELE - Volumetrico UNO. Serial: 115200, fin de linea NL o CR.
   Ver README.md antes de acoplar brazos. No hay realimentacion de posicion. */
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

const byte TRIG[3]={2,4,6}, ECHO[3]={3,5,7}, SPIN[2]={8,9};
// Canales fisicos sin cambiar cableado: X=altura, Y=largo, Z=ancho.
const byte HEIGHT_AXIS=0, LENGTH_AXIS=1, WIDTH_AXIS=2;
const uint32_t MAGIC=0x564F4C31UL;
struct ServoConfig { byte low, high, rest, push, flags; };
struct Config { uint32_t magic; byte version; float ref[3]; ServoConfig s[2]; uint16_t crc; };
Config cfg;
Servo motors[2];
LiquidCrystal_I2C lcd27(0x27,16,2), lcd3f(0x3F,16,2);
LiquidCrystal_I2C *lcd=NULL;
int pos[2]={90,90}, target[2]={90,90};
bool known[2]={false,false}, stopped=false, result=false;
// 0 idle, 1 manual move, 2 rest, 3 push1, 4 push2, 5 retract,
// 6 settle, 7 acquire. No automatic movement at boot.
byte state=0, axis=0, count=0;
float samples[5], distanceCm[3], dims[3];
unsigned long tick=0, since=0, pingAt=0, screenAt=0;
bool page=false;
bool liveSensors=false;
byte liveAxis=0;
unsigned long liveAt=0;
char line[80]; byte used=0; bool overflow=false;

uint16_t checksum(const Config &c){
  uint16_t crc=0xFFFF; const byte *p=(const byte*)&c;
  for(unsigned int i=0;i<offsetof(Config,crc);i++){
    crc^=p[i]; for(byte b=0;b<8;b++) crc=(crc&1)?(crc>>1)^0xA001:crc>>1;
  } return crc;
}
bool valid(const Config &c){
  if(c.magic!=MAGIC || c.version!=1 || c.crc!=checksum(c)) return false;
  for(byte i=0;i<3;i++) if(!isfinite(c.ref[i])||c.ref[i]<4||c.ref[i]>100) return false;
  for(byte i=0;i<2;i++){
    const ServoConfig &s=c.s[i];
    if(s.low>=s.high||s.high>180||s.flags>3) return false;
    if((s.flags&1)&&(s.rest<s.low||s.rest>s.high)) return false;
    if((s.flags&2)&&(s.push<s.low||s.push>s.high)) return false;
  } return true;
}
void defaults(){
  memset(&cfg,0,sizeof(cfg)); cfg.magic=MAGIC; cfg.version=1;
  cfg.ref[0]=17.5; cfg.ref[1]=26; cfg.ref[2]=16;
  for(byte i=0;i<2;i++){cfg.s[i].low=20;cfg.s[i].high=160;cfg.s[i].rest=90;cfg.s[i].push=90;}
}
void screen(const __FlashStringHelper *a,const __FlashStringHelper *b){
  if(!lcd)return; lcd->clear();lcd->print(a);lcd->setCursor(0,1);lcd->print(b);
}
void detachAll(){for(byte i=0;i<2;i++){motors[i].detach();known[i]=false;target[i]=pos[i];}}
void fail(const __FlashStringHelper *why){
  state=0;result=false;detachAll();Serial.print(F("ERR "));Serial.println(why);
  screen(F("Error / revisar"),F("Monitor serie"));
}
void showConfig(){
  Serial.println(F("CONFIG FW=1.1.0 X=ALTURA Y=LARGO Z=ANCHO"));
  for(byte i=0;i<3;i++){Serial.print(F("CAL "));Serial.print((char)('X'+i));Serial.print(' ');Serial.println(cfg.ref[i],2);}
  for(byte i=0;i<2;i++){
    Serial.print(F("SERVO "));Serial.print(i+1);Serial.print(F(" LIMITES "));Serial.print(cfg.s[i].low);Serial.print(' ');Serial.print(cfg.s[i].high);
    Serial.print(F(" REPOSO "));Serial.print(cfg.s[i].rest);Serial.print(F(" EMPUJE "));Serial.print(cfg.s[i].push);
    Serial.print(F(" FLAGS "));Serial.print(cfg.s[i].flags);Serial.print(F(" ACOPLADO "));Serial.print(motors[i].attached());
    Serial.print(F(" ANGULO_ORDENADO "));Serial.println(pos[i]);
  }
  Serial.print(F("ESTADO "));Serial.print(state);Serial.print(F(" STOP "));Serial.println(stopped);
}
bool number(const char *s,float &v){if(!s||!*s)return false;char *end;v=strtod(s,&end);return !*end&&isfinite(v);}
bool integer(const char *s,int &v){float f;if(!number(s,f)||f<0||f>180||floor(f)!=f)return false;v=(int)f;return true;}
bool ready(){return cfg.s[0].flags==3&&cfg.s[1].flags==3&&cfg.s[0].rest!=cfg.s[0].push&&cfg.s[1].rest!=cfg.s[1].push;}
bool atTargets(){return pos[0]==target[0]&&pos[1]==target[1];}
void restTargets(){for(byte i=0;i<2;i++)target[i]=cfg.s[i].rest;}
void startRead(){result=false;axis=0;count=0;state=7;pingAt=millis();screen(F("Midiendo..."),F("Objeto quieto"));}
void help(){
  Serial.println(F("AYUDA | CONFIG VER | ESTADO | CAL VER | CAL X|Y|Z cm"));
  Serial.println(F("GUARDAR | CAL GUARDAR | CARGAR | RESTABLECER"));
  Serial.println(F("SERVO n ACOPLAR angulo (primer movimiento puede ser brusco)"));
  Serial.println(F("SERVO n IR angulo | SERVO n LIMITES min max"));
  Serial.println(F("SERVO n FIJAR REPOSO|EMPUJE | SERVO n LIBERAR"));
  Serial.println(F("LEER (sin mover) | MEDIR (ciclo) | STOP | REANUDAR"));
  Serial.println(F("VIVO ON|OFF (distancias sin filtro, pausa durante movimientos)"));
  Serial.println(F("Calibracion cm, punto decimal. Flags: 1 reposo, 2 empuje, 3 ambos."));
}
void command(char *buf){
  for(char *p=buf;*p;p++) *p=toupper((unsigned char)*p);
  char *a[6];byte n=0;char *p=strtok(buf," \t");
  while(p&&n<6){a[n++]=p;p=strtok(NULL," \t");}
  if(!n)return;
  if(p){Serial.println(F("ERR demasiados argumentos"));return;}
  if(n==1&&!strcmp(a[0],"STOP")){
    state=0;result=false;stopped=true;liveSensors=false;detachAll();Serial.println(F("OK STOP pulsos desactivados"));screen(F("STOP"),F("Servos libres"));return;
  }
  if(n==1&&!strcmp(a[0],"AYUDA")){help();return;}
  if((n==1&&!strcmp(a[0],"ESTADO"))||(n==2&&(!strcmp(a[0],"CONFIG")||!strcmp(a[0],"CAL"))&&!strcmp(a[1],"VER"))){showConfig();return;}
  if(state){Serial.println(F("ERR ocupado; STOP para interrumpir"));return;}
  if(n==2&&!strcmp(a[0],"VIVO")&&(!strcmp(a[1],"ON")||!strcmp(a[1],"OFF"))){
    if(stopped){Serial.println(F("ERR STOP activo"));return;}
    liveSensors=!strcmp(a[1],"ON");result=false;
    screen(liveSensors?F("Sensores en vivo"):F("Lectura detenida"),F("Monitor / app"));
    Serial.println(liveSensors?F("OK VIVO ON"):F("OK VIVO OFF"));return;
  }
  if(n==1&&!strcmp(a[0],"REANUDAR")){stopped=false;Serial.println(F("OK habilitado, servos siguen libres"));screen(F("Listo"),F("LEER / calibrar"));return;}
  if((n==1&&!strcmp(a[0],"GUARDAR"))||(n==2&&!strcmp(a[0],"CAL")&&!strcmp(a[1],"GUARDAR"))){
    cfg.crc=checksum(cfg);EEPROM.put(0,cfg);Config check;EEPROM.get(0,check);
    if(valid(check)&&memcmp(&check,&cfg,sizeof(cfg))==0)Serial.println(F("OK EEPROM guardada"));else Serial.println(F("ERR EEPROM"));return;
  }
  if(n==1&&!strcmp(a[0],"CARGAR")){
    Config tmp;EEPROM.get(0,tmp);if(!valid(tmp)){Serial.println(F("ERR EEPROM invalida; configuracion actual conservada"));return;}
    detachAll();cfg=tmp;result=false;screen(F("Config cargada"),F("Servos libres"));Serial.println(F("OK CARGAR"));return;
  }
  if(n==1&&!strcmp(a[0],"RESTABLECER")){detachAll();defaults();result=false;screen(F("Valores iniciales"),F("Sin guardar"));Serial.println(F("OK valores iniciales en RAM; EEPROM intacta"));return;}
  if(n==3&&!strcmp(a[0],"CAL")){
    float v;int i=a[1][0]-'X';if(strlen(a[1])!=1||i<0||i>2||!number(a[2],v)||v<4||v>100){Serial.println(F("ERR CAL eje X/Y/Z, cm 4..100"));return;}
    cfg.ref[i]=v;result=false;screen(F("Cal actualizada"),F("Falta GUARDAR"));Serial.println(F("OK CAL RAM"));return;
  }
  if(n==1&&!strcmp(a[0],"LEER")){
    if(stopped){Serial.println(F("ERR STOP activo"));return;}
    for(byte i=0;i<2;i++)if(motors[i].attached()){Serial.println(F("ERR liberar ambos servos antes de LEER"));return;}
    startRead();Serial.println(F("OK LEER"));return;
  }
  if(n==1&&!strcmp(a[0],"MEDIR")){
    if(stopped||!ready()||!known[0]||!known[1]||!motors[0].attached()||!motors[1].attached()){
      Serial.println(F("ERR calibrar REPOSO/EMPUJE y ACOPLAR ambos; revisar STOP"));return;
    }
    result=false;restTargets();state=2;screen(F("Alineando..."),F("STOP para parar"));Serial.println(F("OK MEDIR"));return;
  }
  if(n>=3&&!strcmp(a[0],"SERVO")){
    int id;if(!integer(a[1],id)||id<1||id>2){Serial.println(F("ERR servo 1 o 2"));return;}byte i=id-1;
    if(n==3&&!strcmp(a[2],"LIBERAR")){motors[i].detach();known[i]=false;Serial.println(F("OK LIBERAR"));return;}
    int v,w;
    if(n==5&&!strcmp(a[2],"LIMITES")){
      if(!integer(a[3],v)||!integer(a[4],w)||v>=w||motors[i].attached()){Serial.println(F("ERR limites 0..180, min<max; liberar servo"));return;}
      cfg.s[i].low=v;cfg.s[i].high=w;cfg.s[i].flags=0;Serial.println(F("OK LIMITES; volver a fijar posiciones"));return;
    }
    if(n==4&&(!strcmp(a[2],"IR")||!strcmp(a[2],"ACOPLAR"))){
      if(stopped||!integer(a[3],v)||v<cfg.s[i].low||v>cfg.s[i].high){Serial.println(F("ERR STOP o angulo fuera de limites"));return;}
      if(!strcmp(a[2],"ACOPLAR")){
        if(motors[i].attached()){Serial.println(F("ERR ya acoplado; usar IR"));return;}
        motors[i].write(v);motors[i].attach(SPIN[i]);pos[i]=target[i]=v;known[i]=true;
        Serial.println(F("OK ACOPLAR; posicion ordenada, no verificada"));
      }else{
        if(!known[i]||!motors[i].attached()){Serial.println(F("ERR usar ACOPLAR primero"));return;}
        target[i]=v;state=1;Serial.println(F("OK IR"));
      }result=false;return;
    }
    if(n==4&&!strcmp(a[2],"FIJAR")&&(!strcmp(a[3],"REPOSO")||!strcmp(a[3],"EMPUJE"))){
      if(!known[i]||!motors[i].attached()){Serial.println(F("ERR acoplar y ajustar primero"));return;}
      if(!strcmp(a[3],"REPOSO")){cfg.s[i].rest=pos[i];cfg.s[i].flags|=1;}else{cfg.s[i].push=pos[i];cfg.s[i].flags|=2;}
      Serial.println(F("OK posicion en RAM; falta GUARDAR"));return;
    }
  }
  Serial.println(F("ERR comando; AYUDA"));
}
void serialTask(){
  // Bounded work keeps movement responsive even with a noisy serial sender.
  byte budget=32;
  while(Serial.available()&&budget--){char c=Serial.read();
    if(c=='\r'||c=='\n'){
      if(overflow)Serial.println(F("ERR linea demasiado larga"));
      else if(used){line[used]=0;command(line);}used=0;overflow=false;
    }else if(!overflow){if(used<sizeof(line)-1)line[used++]=c;else overflow=true;}
  }
}
void movementTask(){
  if(!state||state>=6)return;
  if(millis()-tick<20)return;tick=millis();
  for(byte i=0;i<2;i++)if(motors[i].attached()&&pos[i]!=target[i]){pos[i]+=target[i]>pos[i]?1:-1;motors[i].write(pos[i]);}
  if(!atTargets())return;
  switch(state){
    case 1:state=0;Serial.println(F("OK movimiento terminado (orden)"));break;
    case 2:target[0]=cfg.s[0].push;state=3;break;
    case 3:target[1]=cfg.s[1].push;state=4;break;
    case 4:restTargets();state=5;break;
    case 5:detachAll();since=millis();state=6;screen(F("Estabilizando"),F("Brazos retirados"));break;
  }
}
void measurementTask(){
  if(state==6&&millis()-since>=800){startRead();return;}
  if(state!=7||millis()-pingAt<70)return;pingAt=millis();liveAt=pingAt;
  digitalWrite(TRIG[axis],LOW);delayMicroseconds(2);digitalWrite(TRIG[axis],HIGH);delayMicroseconds(10);digitalWrite(TRIG[axis],LOW);
  unsigned long us=pulseIn(ECHO[axis],HIGH,12000UL);
  float d=us/58.0;
  if(!us||d<2||d>cfg.ref[axis]+0.5){Serial.print(F("EJE "));Serial.println((char)('X'+axis));fail(F("sin eco o fuera de rango"));return;}
  samples[count++]=d;if(count<5)return;
  for(byte i=1;i<5;i++){float v=samples[i];int j=i-1;while(j>=0&&samples[j]>v){samples[j+1]=samples[j];j--;}samples[j+1]=v;}
  if(samples[4]-samples[0]>0.5){fail(F("lecturas inestables >0.5cm"));return;}
  distanceCm[axis]=samples[2];dims[axis]=cfg.ref[axis]-distanceCm[axis];
  if(dims[axis]<0.5){fail(F("objeto ausente o dimension <0.5cm"));return;}
  count=0;axis++;if(axis<3)return;
  state=0;result=true;screenAt=0;page=false;
  Serial.print(F("RESULTADO L="));Serial.print(dims[LENGTH_AXIS],2);Serial.print(F(" A="));Serial.print(dims[WIDTH_AXIS],2);Serial.print(F(" H="));Serial.print(dims[HEIGHT_AXIS],2);
  Serial.print(F(" cm V="));Serial.print(dims[0]*dims[1]*dims[2],2);Serial.println(F(" cm3"));
  Serial.print(F("DISTANCIAS "));for(byte i=0;i<3;i++){Serial.print(distanceCm[i],2);Serial.print(' ');}Serial.println();
}
void displayTask(){
  if(!lcd||!result)return;if(screenAt&&millis()-screenAt<3000)return;screenAt=millis();lcd->clear();
  if(!page){lcd->print(F("Largo:"));lcd->print(dims[LENGTH_AXIS],1);lcd->print(F(" cm"));lcd->setCursor(0,1);lcd->print(F("Ancho:"));lcd->print(dims[WIDTH_AXIS],1);lcd->print(F(" cm"));}
  else{lcd->print(F("Alto:"));lcd->print(dims[HEIGHT_AXIS],1);lcd->print(F(" cm"));lcd->setCursor(0,1);lcd->print(F("V:"));lcd->print(dims[0]*dims[1]*dims[2],1);lcd->print(F(" cm3"));}page=!page;
}
void setup(){
  Serial.begin(115200);for(byte i=0;i<3;i++){pinMode(TRIG[i],OUTPUT);digitalWrite(TRIG[i],LOW);pinMode(ECHO[i],INPUT);}
  Wire.begin();Wire.setWireTimeout(3000,true);
  Wire.beginTransmission(0x27);if(Wire.endTransmission()==0)lcd=&lcd27;
  if(!lcd){Wire.beginTransmission(0x3F);if(Wire.endTransmission()==0)lcd=&lcd3f;}
  if(lcd){lcd->init();lcd->backlight();}else Serial.println(F("AVISO LCD no detectado en 0x27/0x3F"));
  EEPROM.get(0,cfg);if(!valid(cfg)){defaults();Serial.println(F("AVISO EEPROM invalida/vacia; calibrar servos"));}else Serial.println(F("OK EEPROM recuperada"));
  screen(F("Volumetrico UNO"),F("Servos libres"));help();showConfig();
}
void liveTask(){
  if(!liveSensors||state||stopped||millis()-liveAt<100)return;
  liveAt=millis();byte i=liveAxis;liveAxis=(liveAxis+1)%3;
  bool stuck=digitalRead(ECHO[i])==HIGH;unsigned long us=0;
  if(!stuck){
    digitalWrite(TRIG[i],LOW);delayMicroseconds(2);digitalWrite(TRIG[i],HIGH);delayMicroseconds(10);digitalWrite(TRIG[i],LOW);
    us=pulseIn(ECHO[i],HIGH,30000UL);
  }
  Serial.print(F("SENSOR "));Serial.print((char)('X'+i));Serial.print(F(" ECO_US="));Serial.print(us);Serial.print(F(" CM="));
  if(us)Serial.print(us/58.0,2);else Serial.print(F("NA"));
  Serial.print(F(" STATUS="));
  if(stuck)Serial.println(F("ECHO_ALTO"));else if(!us)Serial.println(F("SIN_ECO"));
  else if(us/58.0<2||us/58.0>400)Serial.println(F("FUERA_NOMINAL"));
  else if(us/58.0>cfg.ref[i]+0.5)Serial.println(F("MAS_ALLA_REFERENCIA"));else Serial.println(F("ECO"));
}
void loop(){serialTask();movementTask();measurementTask();displayTask();liveTask();}
