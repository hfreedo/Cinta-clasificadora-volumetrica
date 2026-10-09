/*
 * Cinta transportadora SCS - Clasificadora por Color
 * Arduino UNO · Firmware principal v2.0
 */

#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>

// ─── PINES ─────────────────────────────────────────────────

#define ENA         5
#define IN1         7
#define IN2         8
#define SERVO1_PIN  10
#define SERVO2_PIN  11
#define TRIG_PIN    13
#define ECHO_PIN    12
#define IR1_PIN     2
#define IR2_PIN     A3
#define TCS_S2      A0
#define TCS_S3      A1
#define TCS_OUT     4
#define TL_RED      9
#define TL_YELLOW   6
#define TL_GREEN   A2
#define BUZZER_PIN  3

// ─── OBJETOS ───────────────────────────────────────────────

Servo servo1;
Servo servo2;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── ESTADOS ──────────────────────────────────────────────

enum Estado {
  REPOSO,
  ARRANQUE_SUAVE,
  BUSCANDO_OBJETO,
  PAUSA_DETECCION,
  LEYENDO_COLOR,
  MOSTRANDO_DECISION,
  RECORRIDO_FINAL,
  MODO_TEST
};
Estado estado = REPOSO;
char lcdLine2[17];

// ─── PARAMETROS ───────────────────────────────────────────

const int PWM_DEMO_MIN = 0;
const int PWM_DEMO_NORMAL = 180;
const int PWM_DEMO_ARRANQUE = 180;
const int PWM_DEMO_MAX = 255;

int velMotor       = PWM_DEMO_NORMAL;
int voltMotor      = 12;    // Referencia informativa: la demostracion usa fuente de 12 V.
int direccion      = 1;
int angRepS1       = 105;
int angRepS2       = 100;
int angActivo      = 16;
int umbralUS       = 10;
// Los objetos desviados recorren toda la zona de salida; el rojo sigue recto
// y llega antes al final de la cinta.
unsigned long recorridoFinalMs = 6000;
const unsigned long RECORRIDO_ROJO_MS = 3000;
// Solo se rechaza un empate practicamente exacto entre ambas referencias.
const int MARGEN_ROJO_AMARILLO = 8;

// Tiempos de demostracion: hacen visible cada fase del clasificador.
const unsigned long DEMO_ARRANQUE_MS     = 700;
const unsigned long DEMO_DETECCION_MS    = 900;
const unsigned long DEMO_LECTURA_MS      = 1200;
const unsigned long DEMO_DECISION_MS     = 1200;

// ─── SENSORES ─────────────────────────────────────────────

int  rVal=0, gVal=0, bVal=0;
char colorDet[9];
float dist = -1;
bool ir1=false, ir2=false;
int  tlState=0;

// ─── FLAGS ────────────────────────────────────────────────

bool usOk = false;
bool hayIR2  = false;
bool modoTest = false;
bool envioOn = true;
bool motorActivo = false;
bool ir1Armado = false;
volatile bool ir1Evento = false;

// ─── CONTADORES ───────────────────────────────────────────

unsigned int cRojo=0, cVerde=0, cAzul=0, cAmarillo=0;
bool calibrado=false;
bool calibradoAmarillo=false;
int calR[4]={0}, calG[4]={0}, calB[4]={0};

// ─── TIMERS ───────────────────────────────────────────────

unsigned long tsData=0, tsUS=0, tsClasif=0, tsBlink=0;

// ─── SETUP ────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println(F("Cinta transportadora SCS v2.0 iniciando..."));
  Serial.flush();

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  detenerMotor();
  servo1.attach(SERVO1_PIN); servo2.attach(SERVO2_PIN);
  servo1.write(angRepS1); servo2.write(angRepS2);

  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(IR1_PIN, INPUT_PULLUP); pinMode(IR2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR1_PIN), registrarIR1, FALLING);
  pinMode(TCS_S2, OUTPUT); pinMode(TCS_S3, OUTPUT); pinMode(TCS_OUT, INPUT);
  pinMode(TL_RED, OUTPUT); pinMode(TL_YELLOW, OUTPUT); pinMode(TL_GREEN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT); noTone(BUZZER_PIN);

  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print(F("CINTA SCS"));
  lcd.setCursor(0,1); lcd.print(F("INICIANDO..."));
  delay(300);

  usOk = detectarUS();
  cargarCalibracion();
  setSemaforo(0);
  setLCD2(F("SISTEMA DETENIDO"));

  Serial.print(F("Listo. US:"));
  Serial.print(usOk ? F("OK") : F("N/D"));
  Serial.print(F(" IR2:")); Serial.print(hayIR2 ? F("OK") : F("N/D"));
  Serial.print(F(" Cal:")); Serial.println(calibrado ? F("EEPROM") : F("default"));
  Serial.println(F("Escriba '?' para ayuda."));
}

// ─── LOOP ──────────────────────────────────────────────────

void loop() {
  manejarSerial();
  actualizarBuzzer();
  if(usOk) actualizarUS();

  switch(estado) {
    case REPOSO:              estadoReposo();             break;
    case ARRANQUE_SUAVE:      estadoArranqueSuave();      break;
    case BUSCANDO_OBJETO:     estadoBuscandoObjeto();     break;
    case PAUSA_DETECCION:     estadoPausaDeteccion();     break;
    case LEYENDO_COLOR:       estadoLeyendoColor();       break;
    case MOSTRANDO_DECISION:  estadoMostrandoDecision();  break;
    case RECORRIDO_FINAL:     estadoRecorridoFinal();     break;
    case MODO_TEST:           estadoModoTest();           break;
  }

  if(millis()-tsData >= 100) { enviarJSON(); tsData=millis(); }
}

// ─── ESTADOS ──────────────────────────────────────────────

void estadoReposo() {
  ir1=!digitalRead(IR1_PIN);
  ir2=hayIR2?!digitalRead(IR2_PIN):false;
}

void estadoArranqueSuave() {
  ir1=!digitalRead(IR1_PIN); ir2=hayIR2?!digitalRead(IR2_PIN):false;
  if(procesarDeteccionIR1()) return;
  if(millis()-tsClasif>=DEMO_ARRANQUE_MS) {
    setMotor(true,velMotor,direccion);
    estado=BUSCANDO_OBJETO;
    setSemaforo(2);
    setLCD2(F("BUSCANDO OBJETO"));
  }
}

void estadoBuscandoObjeto() {
  ir1=!digitalRead(IR1_PIN); ir2=hayIR2?!digitalRead(IR2_PIN):false;
  procesarDeteccionIR1();
}

void registrarIR1() {
  ir1Evento=true;
}

bool tomarEventoIR1() {
  noInterrupts();
  bool evento=ir1Evento;
  ir1Evento=false;
  interrupts();
  return evento;
}

bool procesarDeteccionIR1() {
  ir1=!digitalRead(IR1_PIN);
  bool evento=tomarEventoIR1();

  // Si se inicia con el sensor bloqueado, espera que quede libre antes de armarlo.
  if(!ir1Armado) {
    if(!ir1) ir1Armado=true;
    return false;
  }

  // Acepta el nivel activo o un pulso corto registrado por la interrupcion.
  if(!ir1 && !evento) return false;

  detenerMotor();
  estado=PAUSA_DETECCION;
  ir1Armado=false;
  setLCD2(F("OBJETO DETECT."));
  setSemaforo(1);
  iniciarBuzzer(1000,100);
  tsClasif=millis();
  return true;
}

void estadoPausaDeteccion() {
  ir1=!digitalRead(IR1_PIN); ir2=hayIR2?!digitalRead(IR2_PIN):false;
  if(millis()-tsClasif>=DEMO_DETECCION_MS) {
    estado=LEYENDO_COLOR;
    setLCD2(F("LEYENDO COLOR"));
    tsClasif=millis();
  }
}

void estadoLeyendoColor() {
  ir1=!digitalRead(IR1_PIN); ir2=hayIR2?!digitalRead(IR2_PIN):false;
  if(millis()-tsClasif>=DEMO_LECTURA_MS) {
    leerColor(); determinarColor();
    estado=MOSTRANDO_DECISION;
    char msg[17];
    snprintf(msg, sizeof(msg), "COLOR: %s", colorDet);
    setLCD2(msg);
    tsClasif=millis();
  }
}

void estadoMostrandoDecision() {
  ir1=!digitalRead(IR1_PIN); ir2=hayIR2?!digitalRead(IR2_PIN):false;
  if(millis()-tsClasif>=DEMO_DECISION_MS) {
    setMotor(true,velMotor,direccion);
    setSemaforo(2);

    // La cinta y el servo de desvio arrancan a la vez.
    if(strcmp(colorDet,"rojo")==0) {
      setLCD2(F("ROJO: CONTINUA"));
    } else if(strcmp(colorDet,"verde")==0 || strcmp(colorDet,"amarillo")==0) {
      servo1.write(angActivo);
      setLCD2(strcmp(colorDet,"amarillo")==0 ? F("SERVO AMARILLO") : F("SERVO VERDE"));
    } else if(strcmp(colorDet,"azul")==0) {
      servo2.write(angActivo);
      setLCD2(F("SERVO AZUL"));
    } else {
      detenerMotor();
      estado=REPOSO;
      setSemaforo(0);
      setLCD2(F("COLOR NO VALIDO"));
      return;
    }

    estado=RECORRIDO_FINAL;
    tsClasif=millis();
  }
}

void estadoRecorridoFinal() {
  ir1=!digitalRead(IR1_PIN); ir2=hayIR2?!digitalRead(IR2_PIN):false;
  unsigned long duracion = strcmp(colorDet,"rojo")==0 ? RECORRIDO_ROJO_MS : recorridoFinalMs;
  if(millis()-tsClasif>=duracion) {
    // El desvio se mantiene activo hasta que el objeto completa el recorrido.
    servo1.write(angRepS1);
    servo2.write(angRepS2);
    detenerMotor();
    if(strcmp(colorDet,"rojo")==0) cRojo++;
    else if(strcmp(colorDet,"verde")==0) cVerde++;
    else if(strcmp(colorDet,"azul")==0) cAzul++;
    else if(strcmp(colorDet,"amarillo")==0) cAmarillo++;
    estado=REPOSO;
    setSemaforo(0);
    setLCD2(F("CICLO COMPLETO"));
    iniciarBuzzer(1200,100);
    tsClasif=millis();
  }
}

void estadoModoTest() {
  if(millis()-tsBlink>=500) {
    tsBlink=millis();
    digitalWrite(TL_YELLOW, !digitalRead(TL_YELLOW));
    digitalWrite(TL_RED,LOW); digitalWrite(TL_GREEN,LOW);
  }
  ir1=!digitalRead(IR1_PIN); ir2=hayIR2?!digitalRead(IR2_PIN):false;
}

// ─── MOTOR ─────────────────────────────────────────────────

int limitarVelocidadDemo(int v) {
  return constrain(v, PWM_DEMO_MIN, PWM_DEMO_MAX);
}

void iniciarDemostracion() {
  ir1=!digitalRead(IR1_PIN);
  ir1Armado=!ir1;
  noInterrupts();
  ir1Evento=false;
  interrupts();
  estado=ARRANQUE_SUAVE;
  setMotor(true,PWM_DEMO_ARRANQUE,direccion);
  setSemaforo(1);
  setLCD2(F("ARRANCANDO..."));
  tsClasif=millis();
}

void detenerDemostracion() {
  detenerMotor();
  estado=REPOSO;
  setSemaforo(0);
  setLCD2(F("SISTEMA DETENIDO"));
}

void setMotor(bool run, int spd, int dir) {
  if(!run || spd==0) { detenerMotor(); return; }
  // El PWM solicitado es el PWM aplicado. Con 12 V no se compensa por voltaje.
  analogWrite(ENA, constrain(spd,0,255));
  if(dir>=0) { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); }
  else       { digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); }
  motorActivo=true;
}
void detenerMotor() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW); analogWrite(ENA,0);
  motorActivo=false;
}

// ─── SENSORES ──────────────────────────────────────────────

bool detectarUS() {
  for(int i=0;i<3;i++) {
    digitalWrite(TRIG_PIN,LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN,HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN,LOW);
    if(pulseIn(ECHO_PIN,HIGH,15000)>0) return true;
    delay(20);
  }
  return false;
}
void actualizarUS() {
  if(millis()-tsUS<80) return; tsUS=millis();
  digitalWrite(TRIG_PIN,LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN,HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN,LOW);
  long d = pulseIn(ECHO_PIN,HIGH,6000); // 6000 us = ~100cm max, sin bloqueo excesivo
  if(d > 0) {
    dist = d*0.034/2.0;
  } else {
    dist = -1.0;
  }
}
int mediana3(int a, int b, int c) {
  if(a>b) { int t=a; a=b; b=t; }
  if(b>c) { int t=b; b=c; c=t; }
  if(a>b) { int t=a; a=b; b=t; }
  return b;
}

int leerCanalColor(bool s2, bool s3) {
  digitalWrite(TCS_S2,s2); digitalWrite(TCS_S3,s3);
  int a=(int)pulseIn(TCS_OUT,LOW,10000); delay(4);
  int b=(int)pulseIn(TCS_OUT,LOW,10000); delay(4);
  int c=(int)pulseIn(TCS_OUT,LOW,10000);
  return mediana3(a,b,c);
}

void leerColor() {
  // La mediana elimina pulsos aislados sin retrasar de forma perceptible la demostracion.
  rVal=leerCanalColor(LOW,LOW);
  gVal=leerCanalColor(HIGH,HIGH);
  bVal=leerCanalColor(LOW,HIGH);
}
void determinarColor() {
  if(!calibrado) {
    int mn=min(min(rVal,gVal),bVal); int u=30;
    if(abs(rVal-gVal)<=u && bVal-min(rVal,gVal)>u) strcpy(colorDet,"amarillo");
    else if(rVal==mn&&(gVal-rVal>u)&&(bVal-rVal>u)) strcpy(colorDet,"rojo");
    else if(gVal==mn&&(rVal-gVal>u)&&(bVal-gVal>u)) strcpy(colorDet,"verde");
    else if(bVal==mn&&(rVal-bVal>u)&&(gVal-bVal>u)) strcpy(colorDet,"azul");
    else strcpy(colorDet,"ninguno");
    return;
  }
  if(!calibradoAmarillo && abs(rVal-gVal)<=30 && bVal-min(rVal,gVal)>30) {
    strcpy(colorDet,"amarillo");
    return;
  }
  // Distancia Manhattan contra cada referencia
  long d[4];
  int totalRef=calibradoAmarillo?4:3;
  for(int i=0;i<totalRef;i++){
    d[i]=abs(rVal-calR[i])+abs(gVal-calG[i])+abs(bVal-calB[i]);
  }
  int best=0;
  for(int i=1;i<totalRef;i++) if(d[i]<d[best]) best=i;
  int segundo=best==0?1:0;
  for(int i=0;i<totalRef;i++) if(i!=best && d[i]<d[segundo]) segundo=i;

  // Tolerancia 50%: la desviacion total debe ser menor que la mitad de la suma de referencia
  long refSum = (long)calR[best]+(long)calG[best]+(long)calB[best];
  bool rojoAmarilloAmbiguo=calibradoAmarillo &&
    ((best==0 && segundo==3) || (best==3 && segundo==0)) &&
    (d[segundo]-d[best] < MARGEN_ROJO_AMARILLO);
  if(d[best]*2 > refSum || rojoAmarilloAmbiguo) { strcpy(colorDet,"ninguno"); }
  else {
    const char* n[4]={"rojo","verde","azul","amarillo"};
    strcpy(colorDet,n[best]);
  }
}

void imprimirSalidaEvaluada() {
  if(strcmp(colorDet,"rojo")==0) Serial.print(F("RECTA"));
  else if(strcmp(colorDet,"verde")==0 || strcmp(colorDet,"amarillo")==0) Serial.print(F("SERVO 1"));
  else if(strcmp(colorDet,"azul")==0) Serial.print(F("SERVO 2"));
  else Serial.print(F("SIN SALIDA"));
}

void mostrarEvaluacion() {
  leerColor();
  determinarColor();
  Serial.print(F("EVALUACION R:"));Serial.print(rVal);
  Serial.print(F(" G:"));Serial.print(gVal);Serial.print(F(" B:"));Serial.print(bVal);
  Serial.print(F(" -> "));Serial.print(colorDet);
  Serial.print(F(" | SALIDA: "));
  imprimirSalidaEvaluada();
  Serial.println();
}

void enviarEvaluacionJSON() {
  leerColor();
  determinarColor();
  Serial.print(F("{\"tst\":\"evaluar\",\"r\":"));Serial.print(rVal);
  Serial.print(F(",\"g\":"));Serial.print(gVal);Serial.print(F(",\"b\":"));Serial.print(bVal);
  Serial.print(F(",\"nom\":\""));Serial.print(colorDet);
  Serial.print(F("\",\"salida\":\""));
  imprimirSalidaEvaluada();
  Serial.println(F("\"}"));
}

void enviarAck(const __FlashStringHelper* cmd, bool ok, const __FlashStringHelper* msg) {
  Serial.print(F("{\"ack\":\""));Serial.print(cmd);
  Serial.print(F("\",\"ok\":"));Serial.print(ok ? F("true") : F("false"));
  Serial.print(F(",\"msg\":\""));Serial.print(msg);Serial.println(F("\"}"));
}

int indiceColor(const char* nombre) {
  if(strcmp_P(nombre,PSTR("rojo"))==0) return 0;
  if(strcmp_P(nombre,PSTR("verde"))==0) return 1;
  if(strcmp_P(nombre,PSTR("azul"))==0) return 2;
  if(strcmp_P(nombre,PSTR("amarillo"))==0) return 3;
  return -1;
}

void guardarReferenciaActual(int idx) {
  int addr=1+idx*6;
  EEPROM.put(addr,rVal); EEPROM.put(addr+2,gVal); EEPROM.put(addr+4,bVal);
  EEPROM.write(0,0xAA);
  if(idx==3) EEPROM.write(25,0x53);
  cargarCalibracion();
}

void borrarCalibracion() {
  EEPROM.write(0,0);
  EEPROM.write(25,0);
  calibrado=false;
  calibradoAmarillo=false;
  for(int i=0;i<4;i++){calR[i]=0;calG[i]=0;calB[i]=0;}
}

void enviarCalibracionJSON(bool ok=true) {
  Serial.print(F("{\"tst\":\"calibracion\",\"ok\":"));Serial.print(ok?F("true"):F("false"));
  Serial.print(F(",\"cal\":"));Serial.print(calibrado?F("true"):F("false"));
  Serial.print(F(",\"amarillo\":"));Serial.print(calibradoAmarillo?F("true"):F("false"));
  Serial.print(F(",\"refs\":{"));
  for(int i=0;i<4;i++) {
    if(i>0) Serial.print(',');
    Serial.print('"');
    if(i==0) Serial.print(F("rojo"));
    else if(i==1) Serial.print(F("verde"));
    else if(i==2) Serial.print(F("azul"));
    else Serial.print(F("amarillo"));
    Serial.print(F("\":{\"r\":"));Serial.print(calR[i]);
    Serial.print(F(",\"g\":"));Serial.print(calG[i]);Serial.print(F(",\"b\":"));Serial.print(calB[i]);Serial.print('}');
  }
  Serial.println(F("}}"));
}

void cargarCalibracion() {
  calibrado=(EEPROM.read(0)==0xAA);
  calibradoAmarillo=calibrado && EEPROM.read(25)==0x53;
  if(calibrado) {
    int totalRef=calibradoAmarillo?4:3;
    for(int i=0;i<totalRef;i++){
      int addr=1+i*6;
      EEPROM.get(addr,calR[i]); EEPROM.get(addr+2,calG[i]); EEPROM.get(addr+4,calB[i]);
    }
    velMotor=PWM_DEMO_NORMAL;
  }
}

// ─── ACTUADORES ────────────────────────────────────────────

void setSemaforo(int s) {
  tlState=s;
  digitalWrite(TL_RED,s==0); digitalWrite(TL_YELLOW,s==1); digitalWrite(TL_GREEN,s==2);
}
void iniciarBuzzer(int f, int d) { tone(BUZZER_PIN,f,d); }
void actualizarBuzzer() {}
void setLCD2(const char* texto) {
  strncpy(lcdLine2, texto, 16);
  lcdLine2[16] = '\0';
  actualizarLCD2();
}
void setLCD2(const __FlashStringHelper* texto) {
  strncpy_P(lcdLine2, (PGM_P)texto, 16);
  lcdLine2[16] = '\0';
  actualizarLCD2();
}
void actualizarLCD2() {
  lcd.backlight();
  lcd.setCursor(0,0); lcd.print(F("CINTA SCS       "));
  lcd.setCursor(0,1); lcd.print(lcdLine2);
  for(int i=strlen(lcdLine2);i<16;i++) lcd.print(" ");
}

// ─── SERIAL ────────────────────────────────────────────────

void manejarSerial() {
  if(!Serial.available()) return;
  static char buf[80]; static int bl=0;
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='\n'||c=='\r') {
      if(bl==0) continue;
      buf[bl]=0;
      char* p=buf; while(*p==' '||*p=='\t') p++;
      if(*p=='{') procesarJSON(p);
      else procesarTexto(p);
      bl=0;
    } else if(bl<79) buf[bl++]=c;
  }
}

// ─── COMANDOS JSON ─────────────────────────────────────────

void procesarJSON(char* j) {
  // test on/off (funciona siempre)
  if(strstr(j,"\"cmd\":\"test\"")) {
    if(strstr(j,"\"acc\":\"on\"")) {
      if(estado!=MODO_TEST){detenerMotor();estado=MODO_TEST;modoTest=true;setSemaforo(1);tsBlink=millis();setLCD2(F("MODO TEST"));}
      enviarAck(F("test"),true,F("modo test activo"));
      return;
    }
    if(strstr(j,"\"acc\":\"off\"")) {
      detenerDemostracion();modoTest=false;
      enviarAck(F("test"),true,F("modo test detenido"));
      return;
    }
    if(estado==MODO_TEST) { procesarTest(j); return; }
  }
  if(modoTest) return;

  if(strstr(j,"\"cmd\":\"data\"")) {
    if(strstr(j,"\"on\":true")) envioOn=true;
    if(strstr(j,"\"on\":false")) envioOn=false;
    return;
  }
  if(strstr(j,"\"cmd\":\"motor\"")) {
    int v=extraerInt(j,"\"spd\":");
    int d=extraerInt(j,"\"dir\":");
    if(v>=0 && v<=255) velMotor=limitarVelocidadDemo(v);
    if(d==1 || d==-1) direccion=d;
    if(strstr(j,"\"run\":true") || strstr(j,"\"acc\":\"on\"") || strstr(j,"\"run\":1")) {
      estado=REPOSO;
      int spdRun = (velMotor > 0) ? velMotor : PWM_DEMO_NORMAL;
      velMotor = spdRun;
      setMotor(true, spdRun, direccion);
      enviarAck(F("motor"),true,F("motor encendido"));
    } else if(strstr(j,"\"run\":false") || strstr(j,"\"acc\":\"off\"") || strstr(j,"\"run\":0")) {
      detenerMotor();
      enviarAck(F("motor"),true,F("motor detenido"));
    }
    return;
  }
  if(strstr(j,"\"cmd\":\"start\"")) {
    if(estado==REPOSO) { iniciarDemostracion(); enviarAck(F("start"),true,F("demostracion iniciada")); }
    else enviarAck(F("start"),false,F("el sistema no esta en reposo"));
  }
  else if(strstr(j,"\"cmd\":\"stop\"")) {
    detenerDemostracion();
    enviarAck(F("stop"),true,F("demostracion detenida"));
  }
  else if(strstr(j,"\"cmd\":\"speed\"")) {
    int v=extraerInt(j,"\"val\":");
    if(v>=0&&v<=255) {
      velMotor=limitarVelocidadDemo(v);
      if(motorActivo) setMotor(true,velMotor,direccion);
      enviarAck(F("speed"),true,F("velocidad actualizada"));
    }
  }
  else if(strstr(j,"\"cmd\":\"dir\"")) {
    int d=extraerInt(j,"\"val\":"); if(d==1||d==-1){direccion=d;if(motorActivo)setMotor(true,velMotor,direccion);enviarAck(F("dir"),true,F("direccion actualizada"));}
  }
  else if(strstr(j,"\"cmd\":\"servo\"")) {
    int id=extraerInt(j,"\"id\":"); int a=extraerInt(j,"\"ang\":");
    if((id==1||id==2)&&a>=0&&a<=180) { if(id==1){servo1.write(a);angRepS1=a;} else{servo2.write(a);angRepS2=a;} enviarAck(F("servo"),true,F("posicion actualizada")); }
  }
  else if(strstr(j,"\"cmd\":\"cal\"")) {
    int s1=extraerInt(j,"\"sv1\":"); int s2=extraerInt(j,"\"sv2\":");
    int aa=extraerInt(j,"\"angActivo\":");
    int um=extraerInt(j,"\"umbral\":");
    int tr=extraerInt(j,"\"tiempoRecorrido\":");
    if(tr<0) tr=extraerInt(j,"\"tiempoRojo\":"); // Compatibilidad con el panel anterior.
    int hi=extraerInt(j,"\"hayIR2\":");
    if(s1>=0&&s1<=180){angRepS1=s1;servo1.write(s1);} if(s2>=0&&s2<=180){angRepS2=s2;servo2.write(s2);}
    if(aa>=0&&aa<=180) angActivo=aa;
    if(um>=1&&um<=50)umbralUS=um;
    if(tr>=3000&&tr<=10000)recorridoFinalMs=tr;
    if(hi==0||hi==1)hayIR2=(hi==1);
    enviarAck(F("cal"),true,F("configuracion aplicada"));
  }
}

void procesarTest(char* j) {
  if(strstr(j,"\"acc\":\"servo\"")) {
    int id=extraerInt(j,"\"id\":"); int a=extraerInt(j,"\"ang\":");
    if((id==1||id==2)&&a>=0&&a<=180){if(id==1)servo1.write(a);else servo2.write(a);}
  }
  else if(strstr(j,"\"acc\":\"motor\"")) {
    int v=extraerInt(j,"\"vel\":"); if(v>0&&v<=255)setMotor(true,v,1); else detenerMotor();
  }
  else if(strstr(j,"\"acc\":\"semaforo\"")) {
    int c=extraerInt(j,"\"color\":"); if(c>=0&&c<=2)setSemaforo(c);
  }
  else if(strstr(j,"\"acc\":\"buzzer\"")) {
    int f=extraerInt(j,"\"freq\":"); int d=extraerInt(j,"\"dur\":");
    if(f>0&&d>0)iniciarBuzzer(f,d);
  }
  else if(strstr(j,"\"acc\":\"lcd\"")) {
    char l1[17]="",l2[17]="";
    extraerStr(j,"\"l1\":\"",l1,16); extraerStr(j,"\"l2\":\"",l2,16);
    if(l1[0]){lcd.setCursor(0,0);lcd.print(l1);for(int i=strlen(l1);i<16;i++)lcd.print(" ");}
    if(l2[0]){lcd.setCursor(0,1);lcd.print(l2);for(int i=strlen(l2);i<16;i++)lcd.print(" ");}
  }
  else if(strstr(j,"\"acc\":\"color\"")) {
    leerColor();
    Serial.print(F("{\"tst\":\"color\",\"r\":"));Serial.print(rVal);
    Serial.print(F(",\"g\":"));Serial.print(gVal);Serial.print(F(",\"b\":"));Serial.print(bVal);
    Serial.println(F("}"));
  }
  else if(strstr(j,"\"acc\":\"evaluar\"")) {
    enviarEvaluacionJSON();
  }
  else if(strstr(j,"\"acc\":\"calibrar\"")) {
    char nombre[12]="";
    extraerStr(j,"\"color\":\"",nombre,11);
    int idx=indiceColor(nombre);
    if(idx<0) { enviarAck(F("calibrar"),false,F("color no valido")); return; }
    leerColor();
    guardarReferenciaActual(idx);
    enviarCalibracionJSON(true);
    enviarAck(F("calibrar"),true,F("referencia guardada"));
  }
  else if(strstr(j,"\"acc\":\"calibracion\"")) {
    enviarCalibracionJSON(true);
  }
  else if(strstr(j,"\"acc\":\"borrar_calibracion\"")) {
    borrarCalibracion();
    enviarCalibracionJSON(true);
    enviarAck(F("borrar_calibracion"),true,F("calibracion borrada"));
  }
  else if(strstr(j,"\"acc\":\"ir\"")) {
    Serial.print(F("{\"tst\":\"ir\",\"ir1\":"));Serial.print(!digitalRead(IR1_PIN)?F("true"):F("false"));
    if(hayIR2){Serial.print(F(",\"ir2\":"));Serial.print(!digitalRead(IR2_PIN)?F("true"):F("false"));}
    Serial.println(F("}"));
  }
  else if(strstr(j,"\"acc\":\"us\"")) {
    if(usOk){
      digitalWrite(TRIG_PIN,LOW);delayMicroseconds(2);
      digitalWrite(TRIG_PIN,HIGH);delayMicroseconds(10);
      digitalWrite(TRIG_PIN,LOW);
      long d=pulseIn(ECHO_PIN,HIGH,6000);
      float dm=(d>0)?(d*0.034/2.0):-1.0;
      Serial.print(F("{\"tst\":\"us\",\"dist\":"));
      Serial.print(dm,1);
      Serial.println(F("}"));
    }
    else Serial.println(F("{\"tst\":\"us\",\"dist\":-1}"));
  }
}

int extraerInt(char* j, const char* k) {
  char* p=strstr(j,k); if(!p) return -1;
  p+=strlen(k);
  char n[8]=""; int ni=0;
  while(*p && ni<7) { if((*p>='0'&&*p<='9')||*p=='-') n[ni++]=*p; else if(ni>0) break; p++; }
  return ni>0?atoi(n):-1;
}
void extraerStr(char* j, const char* k, char* out, int max) {
  char* p=strstr(j,k); if(!p){out[0]=0;return;}
  p+=strlen(k); int oi=0;
  while(*p && oi<max) { if(*p=='"') break; out[oi++]=*p; p++; }
  out[oi]=0;
}

// ─── COMANDOS TEXTO (Monitor Serie) ───────────────────────

void procesarTexto(char* cmd) {
  if(cmd[0]=='?') {
    Serial.println(F("start|stop|motor on|off|N|speed N|servo I A|color|evaluar|ir|us"));
    Serial.println(F("semaforo N|buzzer F D|lcd L1 L2|test on|off"));
    Serial.println(F("status|json on|off"));
    return;
  }
  char t[16]=""; int ti=0; char* p=cmd;
  while(*p && *p!=' ' && ti<15) t[ti++]=*p++;
  t[ti]=0; while(*p==' ') p++;

  if(strcmp(t,"start")==0) {
    if(estado==REPOSO){iniciarDemostracion();Serial.println(F("OK"));}
    else Serial.println(F("ERR: no en REPOSO"));
  }
  else if(strcmp(t,"stop")==0) {
    detenerDemostracion();Serial.println(F("OK"));
  }
  else if(strcmp(t,"motor")==0) {
    if(strncmp(p,"on",2)==0) {
      char* sp=strchr(p,' ');
      int v = sp ? atoi(sp+1) : velMotor;
      if(v<=0) v = PWM_DEMO_NORMAL;
      velMotor = limitarVelocidadDemo(v);
      setMotor(true,velMotor,direccion);
      Serial.print(F("motor ON spd="));Serial.println(velMotor);
    } else if(strcmp(p,"off")==0) {
      detenerMotor();
      Serial.println(F("motor OFF"));
    } else {
      int v=atoi(p);
      if(v>0&&v<=255){velMotor=limitarVelocidadDemo(v);setMotor(true,velMotor,direccion);Serial.print(F("motor spd="));Serial.println(velMotor);}
      else if(v==0){detenerMotor();Serial.println(F("motor OFF"));}
    }
  }
  else if(strcmp(t,"speed")==0) {
    int v=atoi(p); if(v>=0&&v<=255){velMotor=limitarVelocidadDemo(v);if(motorActivo)setMotor(true,velMotor,direccion);Serial.print(F("speed="));Serial.println(velMotor);}
    else Serial.println(F("USO: speed 0-255"));
  }
  else if(strcmp(t,"servo")==0) {
    int id=atoi(p); while(*p&&*p!=' ')p++; while(*p==' ')p++;
    int a=atoi(p);
    if((id==1||id==2)&&a>=0&&a<=180){if(id==1){servo1.write(a);angRepS1=a;}else{servo2.write(a);angRepS2=a;}Serial.print(F("servo"));Serial.print(id);Serial.print(F("="));Serial.println(a);}
    else Serial.println(F("USO: servo 1 90"));
  }
  else if(strcmp(t,"color")==0) {
    leerColor();
    Serial.print(F("R:"));Serial.print(rVal);Serial.print(F(" G:"));Serial.print(gVal);Serial.print(F(" B:"));Serial.print(bVal);
    determinarColor(); Serial.print(F(" -> "));Serial.println(colorDet);
  }
  else if(strcmp(t,"evaluar")==0) {
    mostrarEvaluacion();
  }
  else if(strcmp(t,"ir")==0) {
    bool i1=!digitalRead(IR1_PIN);
    Serial.print(F("IR1:"));Serial.print(i1?F("ACTIVO"):F("INACTIVO"));
    if(hayIR2){Serial.print(F(" IR2:"));Serial.println(!digitalRead(IR2_PIN)?F("ACTIVO"):F("INACTIVO"));}
    else Serial.println(F(" IR2:N/D"));
  }
  else if(strcmp(t,"us")==0) {
    if(usOk){digitalWrite(TRIG_PIN,LOW);delayMicroseconds(2);digitalWrite(TRIG_PIN,HIGH);delayMicroseconds(10);digitalWrite(TRIG_PIN,LOW);long d=pulseIn(ECHO_PIN,HIGH,6000);float dm=(d>0)?(d*0.034/2.0):-1.0;Serial.print(F("dist="));Serial.print(dm,1);Serial.println(F("cm"));}
    else Serial.println(F("HC-SR04: no conectado"));
  }
  else if(strcmp(t,"semaforo")==0) {
    int s=atoi(p); if(s>=0&&s<=2){setSemaforo(s);Serial.println(s==0?F("ROJO"):s==1?F("AMARILLO"):F("VERDE"));}
  }
  else if(strcmp(t,"buzzer")==0) {
    int f=atoi(p); while(*p&&*p!=' ')p++; while(*p==' ')p++;
    int d=atoi(p);
    if(f>0&&d>0) iniciarBuzzer(f,d); else iniciarBuzzer(2000,200);
    Serial.println(F("OK"));
  }
  else if(strcmp(t,"lcd")==0) {
    char l1[17]="",l2[17]="";
    int i=0; while(*p&&i<16){if(*p==' ')break;l1[i++]=*p++;} l1[i]=0;
    while(*p==' ')p++; i=0;
    while(*p&&i<16){if(*p==' '&&l2[0])break;l2[i++]=*p++;} l2[i]=0;
    if(l1[0]){lcd.setCursor(0,0);lcd.print(l1);for(int j=strlen(l1);j<16;j++)lcd.print(" ");}
    if(l2[0]){lcd.setCursor(0,1);lcd.print(l2);for(int j=strlen(l2);j<16;j++)lcd.print(" ");}
    Serial.println(F("OK"));
  }
  else if(strcmp(t,"test")==0) {
    if(strcmp(p,"on")==0) {
      if(estado!=MODO_TEST){detenerMotor();estado=MODO_TEST;modoTest=true;setSemaforo(1);tsBlink=millis();setLCD2(F("MODO TEST"));Serial.println(F("Test ON"));}
    } else if(strcmp(p,"off")==0) {
      detenerDemostracion();modoTest=false;Serial.println(F("Test OFF"));
    }
  }
  else if(strcmp(t,"volt")==0) {
    Serial.println("PWM directo: demo 12V");
  }
  else if(strcmp(t,"status")==0||strcmp(t,"st")==0) {
    leerColor(); determinarColor();
  }
  else if(strcmp(t,"volt")==0) {
    Serial.println("PWM directo: demo 12V");
  }
  else if(strcmp(t,"status")==0||strcmp(t,"st")==0) {
    leerColor(); determinarColor();
    Serial.print("R:");Serial.print(rVal);Serial.print(" G:");Serial.print(gVal);Serial.print(" B:");Serial.print(bVal);
    Serial.print(" (");Serial.print(colorDet);Serial.println(")");
    Serial.print("IR1:");Serial.print(!digitalRead(IR1_PIN)?"ACT":"INACT");
    if(hayIR2){Serial.print(" IR2:");Serial.println(!digitalRead(IR2_PIN)?"ACT":"INACT");}
    else Serial.println(" IR2:N/D");
    if(usOk){digitalWrite(TRIG_PIN,LOW);delayMicroseconds(2);digitalWrite(TRIG_PIN,HIGH);delayMicroseconds(10);digitalWrite(TRIG_PIN,LOW);long d=pulseIn(ECHO_PIN,HIGH,6000);Serial.print(F("US:"));Serial.print(d>0?(d*0.034/2.0):-1.0,1);Serial.println(F("cm"));}
    else Serial.println("US:N/D");
    Serial.print("Motor:"); Serial.print(motorActivo?"RUN":"STOP"); Serial.print(" PWM:");Serial.print(velMotor);Serial.print(" V:");Serial.println(voltMotor);
    Serial.print("S1:");Serial.print(servo1.read());Serial.print(" S2:");Serial.println(servo2.read());
    Serial.print("Conteo R:");Serial.print(cRojo);Serial.print(" V:");Serial.print(cVerde);Serial.print(" A:");Serial.print(cAzul);Serial.print(" Am:");Serial.println(cAmarillo);
  }
  else if(strcmp(t,"json")==0) {
    if(strcmp(p,"on")==0){envioOn=true;Serial.println("JSON ON");}
    else if(strcmp(p,"off")==0){envioOn=false;Serial.println("JSON OFF");}
    else Serial.println(envioOn?"JSON ON":"JSON OFF");
  }
  else {
    Serial.print("? ");Serial.print(cmd);Serial.println(" (escriba ? para ayuda)");
  }
}

// ─── ENVIO JSON ───────────────────────────────────────────

void imprimirEstadoJSON() {
  switch(estado) {
    case REPOSO:             Serial.print(F("reposo")); break;
    case ARRANQUE_SUAVE:     Serial.print(F("arranque_suave")); break;
    case BUSCANDO_OBJETO:    Serial.print(F("buscando_objeto")); break;
    case PAUSA_DETECCION:    Serial.print(F("pausa_deteccion")); break;
    case LEYENDO_COLOR:      Serial.print(F("leyendo_color")); break;
    case MOSTRANDO_DECISION: Serial.print(F("mostrando_decision")); break;
    case RECORRIDO_FINAL:    Serial.print(F("recorrido_final")); break;
    case MODO_TEST:          Serial.print(F("modo_test")); break;
  }
}

void enviarJSON() {
  if(!envioOn) return;
  Serial.print(F("{\"t\":"));Serial.print(millis());
  Serial.print(F(",\"st\":\"")); imprimirEstadoJSON(); Serial.print(F("\""));
  Serial.print(F(",\"conv\":{\"run\":"));Serial.print(motorActivo?F("true"):F("false"));
  Serial.print(F(",\"spd\":"));Serial.print(velMotor);Serial.print(F(",\"dir\":"));Serial.print(direccion);
  Serial.print(F("},\"sens\":{\"col\":{\"r\":"));Serial.print(rVal);
  Serial.print(F(",\"g\":"));Serial.print(gVal);Serial.print(F(",\"b\":"));Serial.print(bVal);
  Serial.print(F(",\"nom\":\""));Serial.print(colorDet);Serial.print(F("\"},\"dist\":"));
  if(usOk) Serial.print(dist,1); else Serial.print(F("-1"));
  Serial.print(F(",\"ir\":["));Serial.print(ir1?F("true"):F("false"));Serial.print(F(","));
  Serial.print(hayIR2?(ir2?F("true"):F("false")):F("false"));Serial.print(F("]},\"servo\":["));
  Serial.print(servo1.read());Serial.print(F(","));Serial.print(servo2.read());
  Serial.print(F("],\"tl\":"));Serial.print(tlState);
  Serial.print(",\"cnt\":{\"rojo\":");Serial.print(cRojo);
  Serial.print(",\"verde\":");Serial.print(cVerde);Serial.print(",\"azul\":");Serial.print(cAzul);
  Serial.print(",\"amarillo\":");Serial.print(cAmarillo);
  Serial.print("},\"lcd\":\"");Serial.print(lcdLine2);
  Serial.print("\",\"us\":");Serial.print(usOk?"true":"false");
  Serial.print(",\"ir2\":");Serial.print(hayIR2?"true":"false");
  Serial.print(",\"volt\":");Serial.print(voltMotor);
  Serial.print(F(",\"cfg\":{\"sv1\":"));Serial.print(angRepS1);
  Serial.print(F(",\"sv2\":"));Serial.print(angRepS2);Serial.print(F(",\"act\":"));Serial.print(angActivo);
  Serial.print(F(",\"final\":"));Serial.print(recorridoFinalMs);Serial.print(F(",\"red\":"));Serial.print(RECORRIDO_ROJO_MS);
  Serial.print(F(",\"umbral\":"));Serial.print(umbralUS);Serial.print(F(",\"cal\":"));Serial.print(calibrado?F("true"):F("false"));
  Serial.print(F(",\"calAm\":"));Serial.print(calibradoAmarillo?F("true"):F("false"));Serial.print(F(",\"ir2\":"));Serial.print(hayIR2?F("true"):F("false"));Serial.print(F("}"));
  Serial.println("}");
}
