/*
 * Cinta transportadora SCS - Utilidad de Calibracion y Pruebas
 * Subir este sketch temporalmente para testear hardware.
 * Luego volver a conveyor.ino para operacion normal.
 * 
 * Comandos: ? = ayuda, status = sensores, test = pruebas
 */

#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define ENA 5
#define IN1 7
#define IN2 8
#define SV1 10
#define SV2 11
#define TRIG 13
#define ECHO 12
#define IR1 2
#define IR2 A3
#define S2 A0
#define S3 A1
#define OUT 4
#define TLR 9
#define TLY 6
#define TLG A2
#define BUZ 3

Servo s1, s2;
LiquidCrystal_I2C lcd(0x27,16,2);

int ang1=90, ang2=90;
int vel=180;
int volt=9;
bool usOk=false;
int lastR=0, lastG=0, lastB=0;
bool calibrado=false, calibradoAmarillo=false;
int calR[4]={0}, calG[4]={0}, calB[4]={0};
char colorEvaluado[9];
const int MARGEN_ROJO_AMARILLO=8;

void setup() {
  Serial.begin(115200);
  while(!Serial);

  pinMode(ENA,OUTPUT);pinMode(IN1,OUTPUT);pinMode(IN2,OUTPUT);
  digitalWrite(IN1,LOW);digitalWrite(IN2,LOW);analogWrite(ENA,0);
  s1.attach(SV1);s2.attach(SV2);s1.write(ang1);s2.write(ang2);
  pinMode(TRIG,OUTPUT);pinMode(ECHO,INPUT);digitalWrite(TRIG,LOW);
  pinMode(IR1,INPUT_PULLUP);pinMode(IR2,INPUT_PULLUP);
  pinMode(S2,OUTPUT);pinMode(S3,OUTPUT);pinMode(OUT,INPUT);
  pinMode(TLR,OUTPUT);pinMode(TLY,OUTPUT);pinMode(TLG,OUTPUT);
  pinMode(BUZ,OUTPUT);noTone(BUZ);

  lcd.init();lcd.backlight();
  lcd.setCursor(0,0);lcd.print("CINTA SCS");
  lcd.setCursor(0,1);lcd.print("MODO PRUEBAS");
  delay(400);

  usOk=detectarUS();
  cargarCalibracion();
  semaforo(0);
  menu();
}

void loop() {
  if(Serial.available()) {
    String cmd=Serial.readStringUntil('\n');
    cmd.trim();
    if(cmd.length()>0) procesar(cmd);
  }
}

// ═══════════════════════════════════════════════════════════
//  MENU
// ═══════════════════════════════════════════════════════════

void menu() {
  Serial.println();
  Serial.println(F("══════════════════════════════════════"));
  Serial.println(F("  Cinta transportadora SCS - MODO PRUEBAS v1.0"));
  Serial.println(F("══════════════════════════════════════"));
  Serial.print(F("  HC-SR04: "));
  Serial.println(usOk?F("CONECTADO"):F("NO CONECTADO"));
  Serial.println(F("──────────────────────────────────────"));
  Serial.println(F("  ? / menu        - Mostrar esta ayuda"));
  Serial.println(F("  status / st     - Ver todos los sensores"));
  Serial.println(F("  color           - Evaluar TCS3200 con referencias EEPROM"));
  Serial.println(F("  ir              - Leer sensores IR"));
  Serial.println(F("  us              - Leer ultrasonico"));
  Serial.println(F("  motor ON/OFF N  - Encender/apagar motor (PWM 0-255)"));
  Serial.println(F("  servo I A       - Mover servo (1|2) a A grados"));
  Serial.println(F("  semaforo N      - 0=ROJO 1=AMAR 2=VERDE"));
  Serial.println(F("  buzzer F D      - Sonar (freq Hz, dur ms)"));
  Serial.println(F("  lcd L1 / L2     - Mostrar en LCD (max 16 chars)"));
  Serial.println(F("  volt 6|9|12     - Configurar voltaje L298N"));
  Serial.println(F("  calibrar rojo    - Guardar referencia ROJO"));
  Serial.println(F("  calibrar verde   - Guardar referencia VERDE"));
  Serial.println(F("  calibrar azul    - Guardar referencia AZUL"));
  Serial.println(F("  calibrar amarillo - Guardar referencia AMARILLO"));
  Serial.println(F("  calibrar mostrar - Ver referencias guardadas"));
  Serial.println(F("  calibrar borrar  - Borrar calibracion"));
  Serial.println(F("  test            - Modo automatico (parpadea amarillo)"));
  Serial.println(F("══════════════════════════════════════"));
  Serial.println();
}

// ═══════════════════════════════════════════════════════════
//  PROCESADOR DE COMANDOS
// ═══════════════════════════════════════════════════════════

void procesar(String cmd) {
  cmd.toLowerCase();
  if(cmd=="?"||cmd=="menu"||cmd=="help"||cmd=="ayuda") { menu(); return; }
  if(cmd=="status"||cmd=="st") { mostrarStatus(); return; }
  if(cmd=="color") { leerYMostrarColor(); return; }
  if(cmd=="ir") { mostrarIR(); return; }
  if(cmd=="us") { mostrarUS(); return; }

  // motor ON/OFF/velocidad
  if(cmd.startsWith("motor ")) {
    cmd=cmd.substring(6);
    if(cmd.startsWith("on")) {
      int sp=cmd.indexOf(' ');
      int v=vel;
      if(sp>0) v=cmd.substring(sp+1).toInt();
      if(v>0&&v<=255) vel=v;
      motor(vel);
      Serial.print("Motor ON PWM=");Serial.println(vel);
    }
    else if(cmd=="off") { motor(0); Serial.println("Motor OFF"); }
    else { int v=cmd.toInt(); if(v>=0&&v<=255){vel=v;motor(vel);Serial.print("Motor PWM=");Serial.println(vel);} }
    return;
  }
  // servo I A
  if(cmd.startsWith("servo ")) {
    int sp=cmd.indexOf(' ',6);
    int id=cmd.substring(6,sp).toInt();
    int a=(sp>0)?cmd.substring(sp+1).toInt():90;
    if((id==1||id==2)&&a>=0&&a<=180) {
      if(id==1){s1.write(a);ang1=a;}else{s2.write(a);ang2=a;}
      Serial.print("Servo ");Serial.print(id);Serial.print(" -> ");Serial.print(a);Serial.println(" grados");
    } else Serial.println("USO: servo 1 90");
    return;
  }
  // semaforo
  if(cmd.startsWith("semaforo ")) {
    int s=cmd.substring(9).toInt();
    if(s>=0&&s<=2){semaforo(s);Serial.println(s==0?"ROJO":s==1?"AMARILLO":"VERDE");}
    return;
  }
  // buzzer
  if(cmd.startsWith("buzzer ")) {
    String p=cmd.substring(7); int sp=p.indexOf(' '); int f=p.substring(0,sp).toInt(); int d=(sp>0)?p.substring(sp+1).toInt():300;
    if(f>0){tone(BUZ,f,d);Serial.print("Buzzer ");Serial.print(f);Serial.print("Hz ");Serial.print(d);Serial.println("ms");}
    return;
  }
  // lcd
  if(cmd.startsWith("lcd ")) {
    String l1=cmd.substring(4,20); l1.trim();
    lcd.setCursor(0,0);lcd.print("                ");
    lcd.setCursor(0,0);lcd.print(l1.substring(0,16));
    int sep=l1.indexOf('/');
    if(sep>0){
      String l2=l1.substring(sep+1); l2.trim();
      lcd.setCursor(0,1);lcd.print("                ");
      lcd.setCursor(0,1);lcd.print(l2.substring(0,16));
    }
    Serial.println("LCD actualizado");
    return;
  }
  // volt
  if(cmd.startsWith("volt ")) {
    int v=cmd.substring(5).toInt();
    if(v==6||v==9||v==12){volt=v;Serial.print("Voltaje L298N = ");Serial.print(v);Serial.println("V");}
    else Serial.println("USO: volt 6|9|12");
    return;
  }
  // calibrar
  if(cmd.startsWith("calibrar ")) {
    String acc=cmd.substring(9); acc.trim();
    if(acc=="mostrar") { mostrarCalibracion(); return; }
    if(acc=="borrar") { borrarCalibracion(); return; }
    // calibrar rojo/verde/azul/amarillo
    leerYMostrarColor();
    if(acc=="rojo") guardarCal(0);
    else if(acc=="verde") guardarCal(1);
    else if(acc=="azul") guardarCal(2);
    else if(acc=="amarillo") guardarCal(3);
    else Serial.println("USO: calibrar rojo|verde|azul|amarillo|mostrar|borrar");
    return;
  }
  // test
  if(cmd=="test") {
    Serial.println(F("Modo test: semaforo titila amarillo. Escriba 'stop' para salir."));
    testMode();
    return;
  }
  // stop (para salir del modo test)
  if(cmd=="stop") {
    semaforo(0);
    lcd.setCursor(0,1);lcd.print("LISTO           ");
    Serial.println(F("Modo test finalizado."));
    return;
  }

  Serial.print(F("Comando desconocido: "));Serial.println(cmd);
  Serial.println(F("Escriba '?' para ver la ayuda."));
}

// ═══════════════════════════════════════════════════════════
//  SENSORES
// ═══════════════════════════════════════════════════════════

void mostrarStatus() {
  Serial.println();
  Serial.println(F("══════ ESTADO DE SENSORES ══════"));
  leerYMostrarColor();
  mostrarIR();
  mostrarUS();
  Serial.print(F("Motor: PWM="));Serial.print(vel);Serial.print(F(" V="));Serial.print(volt);Serial.println(F("V"));
  Serial.print(F("Servo 1: "));Serial.print(s1.read());Serial.print(F("   Servo 2: "));Serial.println(s2.read());
  Serial.println(F("══════════════════════════════════"));
}

void leerYMostrarColor() {
  lastR=leerCanalColor(LOW,LOW);
  lastG=leerCanalColor(HIGH,HIGH);
  lastB=leerCanalColor(LOW,HIGH);

  Serial.print(F("TCS3200 -> R:"));Serial.print(lastR);
  Serial.print(F(" G:"));Serial.print(lastG);
  Serial.print(F(" B:"));Serial.print(lastB);

  if(!determinarColorCalibrado()) {
    Serial.println(F("  [SIN CALIBRACION]"));
    return;
  }
  mostrarDistanciasColor();
  Serial.print(F("  ["));Serial.print(colorEvaluado);Serial.println(F("]"));
}

int mediana3(int a, int b, int c) {
  if(a>b) { int t=a; a=b; b=t; }
  if(b>c) { int t=b; b=c; c=t; }
  if(a>b) { int t=a; a=b; b=t; }
  return b;
}

int leerCanalColor(bool s2, bool s3) {
  digitalWrite(S2,s2);digitalWrite(S3,s3);
  int a=(int)pulseIn(OUT,LOW,10000);delay(4);
  int b=(int)pulseIn(OUT,LOW,10000);delay(4);
  int c=(int)pulseIn(OUT,LOW,10000);
  return mediana3(a,b,c);
}

void mostrarDistanciasColor() {
  if(!calibrado || !calibradoAmarillo) return;
  long dRojo=abs(lastR-calR[0])+abs(lastG-calG[0])+abs(lastB-calB[0]);
  long dAmarillo=abs(lastR-calR[3])+abs(lastG-calG[3])+abs(lastB-calB[3]);
  Serial.print(F(" D[R]:"));Serial.print(dRojo);
  Serial.print(F(" D[Am]:"));Serial.print(dAmarillo);
  Serial.print(F(" Dif:"));Serial.print(abs(dRojo-dAmarillo));
}

void mostrarIR() {
  bool i1=!digitalRead(IR1);
  bool i2=!digitalRead(IR2);
  Serial.print(F("IR1 (D2): "));Serial.print(i1?F("ACTIVO"):F("INACTIVO"));
  Serial.print(F("   IR2 (A3): "));Serial.println(i2?F("ACTIVO"):F("INACTIVO"));
}

void mostrarUS() {
  if(usOk) {
    digitalWrite(TRIG,LOW);delayMicroseconds(2);
    digitalWrite(TRIG,HIGH);delayMicroseconds(10);
    digitalWrite(TRIG,LOW);
    long d=pulseIn(ECHO,HIGH,30000);
    float dist=d*0.034/2.0;
    Serial.print(F("HC-SR04: "));Serial.print(dist,1);Serial.println(F(" cm"));
  } else {
    Serial.println(F("HC-SR04: NO CONECTADO"));
  }
}

bool detectarUS() {
  for(int i=0;i<3;i++) {
    digitalWrite(TRIG,LOW);delayMicroseconds(2);
    digitalWrite(TRIG,HIGH);delayMicroseconds(10);
    digitalWrite(TRIG,LOW);
    if(pulseIn(ECHO,HIGH,30000)>0) return true;
    delay(50);
  }
  return false;
}

// ═══════════════════════════════════════════════════════════
//  ACTUADORES
// ═══════════════════════════════════════════════════════════

void motor(int pwm) {
  if(pwm==0){digitalWrite(IN1,LOW);digitalWrite(IN2,LOW);analogWrite(ENA,0);return;}
  int p=pwm;
  if(volt==6)p=min(255,(int)(pwm*1.4));
  else if(volt==12)p=(int)(pwm*0.65);
  analogWrite(ENA,constrain(p,0,255));
  digitalWrite(IN1,HIGH);digitalWrite(IN2,LOW);
}

void semaforo(int s) {
  digitalWrite(TLR,s==0?HIGH:LOW);
  digitalWrite(TLY,s==1?HIGH:LOW);
  digitalWrite(TLG,s==2?HIGH:LOW);
}

void testMode() {
  bool enTest=true;
  semaforo(1);
  unsigned long tb=millis();
  while(enTest) {
    if(millis()-tb>500){tb=millis();digitalWrite(TLY,!digitalRead(TLY));}
    if(Serial.available()) {
      String c=Serial.readStringUntil('\n'); c.trim(); c.toLowerCase();
      if(c=="stop"){enTest=false;semaforo(0);}
      else if(c.startsWith("servo")||c.startsWith("motor")||c.startsWith("buzzer")||
              c.startsWith("lcd")||c=="color"||c=="ir"||c=="us"||c=="status") {
        procesar(c);
      }
    }
  }
}

bool determinarColorCalibrado() {
  if(!calibrado) return false;

  if(!calibradoAmarillo && abs(lastR-lastG)<=30 && lastB-min(lastR,lastG)>30) {
    strcpy(colorEvaluado,"amarillo");
    return true;
  }

  long d[4];
  int totalRef=calibradoAmarillo?4:3;
  for(int i=0;i<totalRef;i++) {
    d[i]=abs(lastR-calR[i])+abs(lastG-calG[i])+abs(lastB-calB[i]);
  }

  int best=0;
  for(int i=1;i<totalRef;i++) if(d[i]<d[best]) best=i;
  int segundo=best==0?1:0;
  for(int i=0;i<totalRef;i++) if(i!=best && d[i]<d[segundo]) segundo=i;

  long refSum=(long)calR[best]+(long)calG[best]+(long)calB[best];
  bool rojoAmarilloAmbiguo=calibradoAmarillo &&
    ((best==0 && segundo==3) || (best==3 && segundo==0)) &&
    (d[segundo]-d[best]<MARGEN_ROJO_AMARILLO);
  if(d[best]*2>refSum || rojoAmarilloAmbiguo) {
    strcpy(colorEvaluado,"ninguno");
  } else {
    const char* nombres[]={"rojo","verde","azul","amarillo"};
    strcpy(colorEvaluado,nombres[best]);
  }
  return true;
}

void cargarCalibracion() {
  calibrado=(EEPROM.read(0)==0xAA);
  calibradoAmarillo=calibrado && EEPROM.read(25)==0x53;
  if(!calibrado) return;

  int totalRef=calibradoAmarillo?4:3;
  for(int i=0;i<totalRef;i++) {
    int addr=1+i*6;
    EEPROM.get(addr,calR[i]); EEPROM.get(addr+2,calG[i]); EEPROM.get(addr+4,calB[i]);
  }
}

// ─── EEPROM CALIBRACION ──────────────────────────────────

void guardarCal(int idx) {
  int addr = 1 + idx * 6;
  EEPROM.put(addr, lastR); EEPROM.put(addr+2, lastG); EEPROM.put(addr+4, lastB);
  EEPROM.write(0, 0xAA);
  if(idx==3) EEPROM.write(25, 0x53);
  cargarCalibracion();
  const char* nombres[]={"ROJO","VERDE","AZUL","AMARILLO"};
  Serial.print("Referencia "); Serial.print(nombres[idx]);
  Serial.print(" guardada: R:"); Serial.print(lastR);
  Serial.print(" G:"); Serial.print(lastG);
  Serial.print(" B:"); Serial.println(lastB);
}

void mostrarCalibracion() {
  if(EEPROM.read(0)!=0xAA){ Serial.println("Sin calibracion guardada."); return; }
  Serial.println("--- REFERENCIAS GUARDADAS ---");
  Serial.print("ROJO:  "); mostrarRef(0);
  Serial.print("VERDE: "); mostrarRef(1);
  Serial.print("AZUL:  "); mostrarRef(2);
  Serial.print("AMARILLO: "); mostrarRef(3);
}
void mostrarRef(int idx) {
  int r,g,b; int addr=1+idx*6;
  EEPROM.get(addr,r); EEPROM.get(addr+2,g); EEPROM.get(addr+4,b);
  Serial.print("R:");Serial.print(r);Serial.print(" G:");Serial.print(g);Serial.print(" B:");Serial.println(b);
}
void borrarCalibracion() {
  EEPROM.write(0,0);
  EEPROM.write(25,0);
  cargarCalibracion();
  Serial.println("Calibracion borrada.");
}
