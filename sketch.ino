// Smart Emergency — ÉTAPE 1 : une zone locale, Arduino C++ pour l'apprentissage.
// Pas de MQTT ni de TLS dans cette étape. Voir le guide pour la version finale ESP-IDF.
#include <Arduino.h>
#include <initializer_list>
#include <DHTesp.h>
#include <esp_system.h>
#include "rules.h"

const int DHT_PIN=15, MQ_PIN=34, FLAME_PIN=27, PIR_PIN=26;
const int DOOR_PIN=25, POWER_PIN=33, ARM_PIN=32, FAULT_PIN=35;
const int RESET_PIN=14, ACK_PIN=13;
const int GREEN_PIN=18, YELLOW_PIN=19, RED_PIN=21, BUZZER_PIN=23, RELAY_PIN=22;
const char* ZONE="zone_01";
const char* DEVICE="esp32_01";
const char* RULES_VERSION="sim-local-1.0";
const uint32_t PERIOD=2000, SIM_WARMUP=4000;
DHTesp dht;
RiskEngine engine;
Sample sample;
uint32_t lastAcquisition=0, seq=0, bootId=0;
bool resetWasPressed=false, ackWasPressed=false, acknowledged=false;
int lastSeverity=NORMAL;

// Interrupteurs avec rebonds neutralisés dans diagram.json pour cette étape.
bool flame(){return digitalRead(FLAME_PIN)==HIGH;}
bool motion(){return digitalRead(PIR_PIN)==HIGH;}
bool door(){return digitalRead(DOOR_PIN)==HIGH;}
bool armed(){return digitalRead(ARM_PIN)==HIGH;}
void printRuleMask(unsigned mask) {
  Serial.print('[');
  bool first=true;
  for(int i=0;i<10;i++) if(mask & (1u<<i)) {
    if(!first) Serial.print(',');
    first=false;
    Serial.printf("\"R%02d\"",i+1);
  }
  Serial.print(']');
}
void printTelemetry(uint32_t now) {
  const Decision& d=engine.result;
  // Heure UTC inconnue à cette étape : timestamp=null, time_valid=false.
  Serial.printf("{\"schema_version\":1,\"building_id\":\"demo_01\",\"zone_id\":\"%s\",\"device_id\":\"%s\",\"boot_id\":\"boot_%08lx\",\"seq\":%lu,\"timestamp\":null,\"time_valid\":false,\"uptime_ms\":%lu,\"simulation\":true,\"measurements\":{\"temperature_c\":",ZONE,DEVICE,(unsigned long)bootId,(unsigned long)++seq,(unsigned long)now);
  if(sample.temperatureValid) Serial.print(sample.temperature,1); else Serial.print("null");
  Serial.print(",\"humidity_pct\":");
  if(sample.temperatureValid) Serial.print(sample.humidity,1); else Serial.print("null");
  Serial.printf(",\"mq2_raw\":%d,\"flame\":%s,\"motion\":%s,\"door_open\":%s,\"source_present\":%s},\"quality\":{\"temperature_valid\":%s,\"mq2_ready\":%s},\"intrusion_armed\":%s,\"rules_version\":\"%s\",\"rssi_dbm\":null,\"local\":{\"severity\":\"%s\",\"health\":\"%s\",\"acknowledged\":%s,\"relay\":%s,\"rules\":",sample.mq,sample.flame?"true":"false",sample.motion?"true":"false",sample.door?"true":"false",sample.source?"true":"false",sample.temperatureValid?"true":"false",sample.mqReady?"true":"false",armed()?"true":"false",RULES_VERSION,severityName(d.severity),!sample.mqReady?"WARMING_UP":(d.healthy?"OK":"DEGRADED"),acknowledged?"true":"false",d.relay?"true":"false");
  printRuleMask(d.ruleMask);
  Serial.print(",\"pending_rules\":"); printRuleMask(d.pendingMask);
  Serial.print(",\"incidents\":{");
  for(int i=0;i<INCIDENT_COUNT;i++) {
    if(i) Serial.print(',');
    Serial.printf("\"%s\":{\"active\":\"%s\",\"memorized_or_active\":\"%s\"}",incidentName(i),severityName(d.active[i]),severityName(d.displayed[i]));
  }
  Serial.println("}}}");
}
void outputs(uint32_t now) {
  const Decision& d=engine.result;
  digitalWrite(GREEN_PIN,d.severity==NORMAL && d.healthy);
  digitalWrite(YELLOW_PIN,d.severity==WARNING);
  digitalWrite(RED_PIN,d.severity>=ALERT);
  // Module Wokwi par défaut : entrée LOW relie COM à NO.
  digitalWrite(RELAY_PIN,d.relay?LOW:HIGH);
  static bool wasSounding=false;
  bool sounding=d.severity==CRITICAL || (d.severity==ALERT && now%1000<500);
  if(sounding!=wasSounding) {
    if(sounding) tone(BUZZER_PIN,1800); else noTone(BUZZER_PIN);
    wasSounding=sounding;
  }
}
void setup() {
  Serial.begin(115200);
  for(int p : {FLAME_PIN,PIR_PIN,DOOR_PIN,POWER_PIN,ARM_PIN,FAULT_PIN}) pinMode(p,INPUT);
  pinMode(RESET_PIN,INPUT_PULLUP); pinMode(ACK_PIN,INPUT_PULLUP);
  for(int p : {GREEN_PIN,YELLOW_PIN,RED_PIN,BUZZER_PIN,RELAY_PIN}) pinMode(p,OUTPUT);
  digitalWrite(RELAY_PIN,HIGH);
  digitalWrite(YELLOW_PIN,HIGH);
  dht.setup(DHT_PIN,DHTesp::DHT22);
  analogReadResolution(12);
  bootId=esp_random();
  engine.result.severity=WARNING;
  Serial.println("# Smart Emergency — simulation locale ; mesures toutes les 2 s.");
  Serial.println("# Potentiometre=ADC injecte MQ2. Interrupteurs: gauche=0, droite=1.");
  Serial.println("# SOURCE: gauche=absente, droite=presente. DEF DHT: droite=invalide.");
  Serial.println("# Prechauffage simule 4 s ; aucune calibration physique n'est validee.");
}
void loop() {
  uint32_t now=millis();
  engine.observe(armed(),motion(),door(),now);
  if(uint32_t(now-lastAcquisition)>=PERIOD) {
    lastAcquisition=now;
    TempAndHumidity th=dht.getTempAndHumidity();
    sample.at=now; sample.temperature=th.temperature; sample.humidity=th.humidity;
    sample.temperatureValid=isfinite(th.temperature)&&isfinite(th.humidity)&&digitalRead(FAULT_PIN)==LOW;
    long total=0; for(int i=0;i<8;i++) total+=analogRead(MQ_PIN);
    sample.mq=total/8; sample.mqReady=now>=SIM_WARMUP;
    sample.flame=flame(); sample.motion=motion(); sample.door=door();
    sample.source=digitalRead(POWER_PIN)==HIGH;
    engine.update(sample,armed(),now);
    if(engine.result.severity>lastSeverity) acknowledged=false;
    lastSeverity=engine.result.severity;
    printTelemetry(now);
  }
  bool rp=digitalRead(RESET_PIN)==LOW, ap=digitalRead(ACK_PIN)==LOW;
  if(rp&&!resetWasPressed) {
    bool ok=engine.reset(now);
    if(ok) acknowledged=false;
    Serial.println(ok?"# RESET EXECUTED":"# RESET REJECTED : condition active, mesures invalides ou 3 acquisitions normales manquantes.");
  }
  if(ap&&!ackWasPressed) {
    acknowledged=true;
    Serial.println("# ACK local : lecture confirmee, alarme maintenue. Pas d'identite utilisateur dans cette etape.");
  }
  resetWasPressed=rp; ackWasPressed=ap;
  outputs(now);
  delay(10);
}
