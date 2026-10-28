#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
#if !defined(TESTER_NATIVE) && !defined(__AVR_ATmega328P__)
#error "This firmware supports only the 5 V ATmega328P Uno/classic Nano."
#endif

Probe probes[3]={{A0,2,3},{A1,4,5},{A2,6,7}};
uint8_t inputs[4]={8,9,10,11}, inputCount=2, outputPin=A3;
uint16_t supply=5000, lowOhms=680;
uint32_t highOhms=470000;
uint8_t mode=0; // transistor, gate, diode, resistor
bool configured=false;
class ArduinoHardware : public Hardware {
 public:
  void release() override {
    // INPUT first prevents a high output pulse while releasing a low pin.
    for (uint8_t pin=2; pin<20; ++pin) { pinMode(pin,INPUT); digitalWrite(pin,LOW); }
  }
  void drive(uint8_t pin, bool high) override { digitalWrite(pin,high?HIGH:LOW); pinMode(pin,OUTPUT); }
  void settle(uint16_t ms) override { delay(ms); }
  uint16_t voltage(uint8_t pin) override {
    analogRead(pin); // discard ADC mux acquisition
    delay(3); // high-impedance 470k path: allow acquisition/stray C to settle
    uint32_t sum=0;
    for (uint8_t n=0; n<16; ++n) sum+=analogRead(pin);
    return (sum*supply+8184UL)/16368UL;
  }
} hardware;

void error(const __FlashStringHelper *message) {
  hardware.release(); Serial.print(F("{\"ok\":false,\"error\":\""));
  Serial.print(message); Serial.println(F("\"}"));
}
void ok() { Serial.println(F("{\"ok\":true}")); }
void field(const __FlashStringHelper *key, int value) { Serial.print(key); Serial.print(value); }
void runTest() {
  if (!configured) { error(F("configure mode and wiring first")); return; }
  if (mode==1) {
    hardware.release();
    Serial.print(F("{\"ok\":true,\"mode\":\"gate\",\"rows\":["));
    for (uint8_t row=0; row<(1<<inputCount); ++row) {
      for (uint8_t i=0; i<inputCount; ++i) hardware.drive(inputs[i],(row>>i)&1);
      delay(2);
      uint16_t mv=hardware.voltage(outputPin);
      int level=mv<=supply*0.3f?0:(mv>=supply*0.7f?1:-1);
      if (row) Serial.print(',');
      Serial.print(F("{\"bits\":")); Serial.print(row);
      field(F(",\"level\":"),level); field(F(",\"mv\":"),mv); Serial.print('}');
    }
    hardware.release(); Serial.println(F("]}")); return;
  }
  Analyzer analyzer(hardware,probes,supply,lowOhms,highOhms);
  if (mode==0) {
    Part part=analyzer.transistor();
    Serial.print(F("{\"ok\":true,\"mode\":\"transistor\",\"kind\":\"")); Serial.print(part.kind);
    Serial.print(F("\",\"control\":")); Serial.print(part.control<0?0:part.control+1);
    field(F(",\"collector_or_drain\":"),part.collector<0?0:part.collector+1);
    field(F(",\"emitter_or_source\":"),part.emitter<0?0:part.emitter+1);
    Serial.print(F(",\"hfe_estimate\":")); Serial.print(part.gain,1);
    field(F(",\"vbe_mv\":"),part.junction); Serial.println('}');
  } else if (mode==2) {
    analyzer.scanDiodes();
    Serial.print(F("{\"ok\":true,\"mode\":\"diode\",\"junctions\":["));
    for (uint8_t i=0; i<analyzer.count; ++i) {
      if (i) Serial.print(',');
      Serial.print(F("{\"anode\":")); Serial.print(analyzer.diodes[i].anode+1);
      field(F(",\"cathode\":"),analyzer.diodes[i].cathode+1);
      field(F(",\"mv\":"),analyzer.diodes[i].mv); Serial.print('}');
    }
    Serial.println(F("]}"));
  } else {
    float ohms=analyzer.resistance(0,1);
    Serial.print(F("{\"ok\":true,\"mode\":\"resistor\",\"ohms\":"));
    if (ohms<0) Serial.print(F("null")); else Serial.print(ohms,1);
    Serial.println('}');
  }
  hardware.release();
}

// A bounded line protocol avoids dynamic String allocation on a 2 kB MCU.
char line[160];
uint8_t used=0;
bool overflow=false;
void command(char *text) {
  hardware.release();
  char *tokens[16]; uint8_t count=0;
  for (char *p=strtok(text," \r\t"); p && count<16; p=strtok(NULL," \r\t")) tokens[count++]=p;
  if (!count) { error(F("empty command")); return; }
  if (!strcmp(tokens[0],"HELLO") && count==1) {
    Serial.println(F("{\"ok\":true,\"protocol\":1,\"board\":\"uno328p\"}")); return;
  }
  if (!strcmp(tokens[0],"RUN") && count==1) { runTest(); return; }
  if (!strcmp(tokens[0],"STOP") && count==1) { ok(); return; }
  if (!strcmp(tokens[0],"CONFIG")) {
    configured=false;
    if (count<3) { error(F("CONFIG requires mode and pins")); return; }
    long values[14];
    for (uint8_t i=2; i<count; ++i) {
      char *end; long value=strtol(tokens[i],&end,10);
      if (!tokens[i][0] || *end || value<0 || value>1000000) { error(F("invalid integer")); return; }
      values[i-2]=value;
    }
    if (!strcmp(tokens[1],"gate")) {
      int n=values[0];
      if (n<1 || n>4 || count!=n+4) { error(F("gate requires count, inputs, analog output")); return; }
      bool seen[20]={false};
      for (int i=0; i<n+1; ++i) {
        int pin=values[i+1];
        if (pin<2 || pin>19 || seen[pin] || (i==n && pin<14)) { error(F("invalid or duplicate gate pin")); return; }
        seen[pin]=true;
      }
      inputCount=n;
      for (int i=0; i<n; ++i) inputs[i]=values[i+1];
      outputPin=values[n+1]; mode=1; supply=5000;
    } else {
      if (!strcmp(tokens[1],"transistor")) mode=0;
      else if (!strcmp(tokens[1],"diode")) mode=2;
      else if (!strcmp(tokens[1],"resistor")) mode=3;
      else { error(F("unknown mode")); return; }
      if (count!=14) { error(F("probe mode requires 9 pins, Vcc, Rl, Rh")); return; }
      bool seen[20]={false};
      for (int i=0; i<9; ++i) {
        int pin=values[i];
        if (pin<2 || pin>19 || seen[pin] || (i%3==0 && pin<14)) { error(F("invalid or duplicate probe pin")); return; }
        seen[pin]=true;
      }
      long v=strtol(tokens[11],NULL,10), l=strtol(tokens[12],NULL,10), h=strtol(tokens[13],NULL,10);
      if (v<4500 || v>5500 || l<600 || l>800 || h<400000 || h>550000) { error(F("calibration out of range")); return; }
      for (int i=0; i<3; ++i) probes[i]={(uint8_t)values[3*i],(uint8_t)values[3*i+1],(uint8_t)values[3*i+2]};
      supply=v; lowOhms=l; highOhms=h;
    }
    configured=true; ok(); return;
  }
  error(F("unknown command"));
}
void setup() { hardware.release(); analogReference(DEFAULT); Serial.begin(115200); }
void loop() {
  while (Serial.available()) {
    char c=Serial.read();
    if (c=='\n') {
      if (overflow) { configured=false; error(F("line too long")); }
      else { line[used]=0; command(line); }
      used=0; overflow=false;
    } else if (used<sizeof(line)-1 && !overflow) line[used++]=c;
    else overflow=true;
  }
}
