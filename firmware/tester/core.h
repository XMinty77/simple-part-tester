// Educational adaptation of Frejek/Reschke's probe sequence and common-emitter
// gain calculation; see vendor/NOTICE.md. All voltages below are millivolts.
#pragma once
#include <stdint.h>

struct Probe { uint8_t adc, low, high; };
struct Junction { uint8_t anode, cathode; uint16_t mv; };
struct Part {
  const char *kind;
  int8_t control, collector, emitter;
  float gain;
  uint16_t junction;
};
struct Hardware {
  virtual void release() = 0;
  virtual void drive(uint8_t pin, bool high) = 0;
  virtual uint16_t voltage(uint8_t pin) = 0;
  virtual void settle(uint16_t ms) = 0;
};
class Analyzer {
 public:
  Hardware &io;
  Probe *p;
  uint16_t vcc;
  float rl, rh, pinResistance;
  Junction diodes[6];
  uint8_t count;
  Analyzer(Hardware &h, Probe *pins, uint16_t supply=5000,
           float low=680, float high=470000, float internal=25)
      : io(h), p(pins), vcc(supply), rl(low), rh(high),
        pinResistance(internal), count(0) {}
  void discharge() {
    io.release();
    for (uint8_t i=0; i<3; ++i) io.drive(p[i].low, false);
    io.settle(20);
    io.release();
  }
  bool diode(uint8_t a, uint8_t c) {
    for (uint8_t i=0; i<count; ++i)
      if (diodes[i].anode==a && diodes[i].cathode==c) return true;
    return false;
  }
  void scanDiodes() {
    count=0;
    for (uint8_t a=0; a<3; ++a) for (uint8_t c=0; c<3; ++c) {
      if (a==c) continue;
      discharge();
      io.drive(p[c].adc, false);
      io.drive(p[a].low, true);
      io.settle(10);
      uint16_t va=io.voltage(p[a].adc), vc=io.voltage(p[c].adc);
      if (va>vc+150 && va<vc+4000 && vcc-va>100)
        diodes[count++]={a,c,(uint16_t)(va-vc)};
    }
    io.release();
  }
  // c/e are collector/emitter for BJTs, drain/source for MOSFETs.
  uint16_t bias(uint8_t c, uint8_t e, uint8_t b, bool npn,
                bool on, bool weak) {
    discharge();
    io.drive(p[e].adc, !npn);
    io.drive(p[c].low, npn);
    io.drive(weak ? p[b].high : p[b].low, on ? npn : !npn);
    io.settle(weak ? 80 : 20);
    uint16_t vc=io.voltage(p[c].adc);
    return npn ? vcc-vc : vc;
  }
  Part transistor() {
    scanDiodes();
    Part best={"unknown",-1,-1,-1,0,0};
    for (uint8_t b=0; b<3; ++b) for (uint8_t e=0; e<3; ++e) {
      if (b==e) continue;
      uint8_t c=3-b-e;
      for (uint8_t polarity=0; polarity<2; ++polarity) {
        bool n=polarity==0;
        uint16_t off=bias(c,e,b,n,false,false);
        if (off>200) continue; // Reject shorts, body diode and depletion devices.
        uint16_t on=bias(c,e,b,n,true,true);
        uint16_t vb=io.voltage(p[b].adc), ve=io.voltage(p[e].adc);
        uint16_t baseDrop=n ? vcc-vb : vb;
        int16_t vbe=n ? (int16_t)vb-ve : (int16_t)ve-vb;
        bool junctions=n ? diode(b,e)&&diode(b,c) : diode(e,b)&&diode(c,b);
        if (junctions && on>off+60 && baseDrop>500 && vbe>350 && vbe<1600) {
          // Reschke semi.c CheckTransistor: subtract off-state leakage;
          // hFE = (U_Rc - U_Rleak) * Rh / (U_Rb * (Rl + Ri)).
          float gain=(on-off)*rh/(baseDrop*(rl+pinResistance));
          if (gain>best.gain) best={n?"NPN":"PNP",(int8_t)b,(int8_t)c,
                                  (int8_t)e,gain,(uint16_t)vbe};
        } else if (!junctions && baseDrop<100 && on>1000 &&
                   (n ? diode(e,c) : diode(c,e))) {
          // A gate must actively turn the channel off again.
          if (bias(c,e,b,n,false,false)<200 && best.gain==0)
            best={n?"N-MOSFET":"P-MOSFET",(int8_t)b,(int8_t)c,(int8_t)e,0,0};
        }
      }
    }
    discharge();
    return best;
  }
  float resistance(uint8_t a, uint8_t b) {
    float result=-1, closest=1e9;
    for (uint8_t range=0; range<2; ++range) {
      float values[2], distance=0;
      bool valid=true;
      for (uint8_t reverse=0; reverse<2; ++reverse) {
        uint8_t hi=reverse?b:a, lo=reverse?a:b;
        discharge();
        io.drive(p[lo].adc,false);
        io.drive(range?p[hi].high:p[hi].low,true);
        io.settle(50);
        uint16_t vh=io.voltage(p[hi].adc), vl=io.voltage(p[lo].adc);
        if (vh>=vcc-25 || vh<vl) { valid=false; break; }
        values[reverse]=(range?rh:rl+pinResistance)*(vh-vl)/(vcc-vh);
        float d=(float)vh-vcc/2;
        distance+=d<0?-d:d;
      }
      if (valid && distance<closest) {
        float mean=(values[0]+values[1])/2;
        float delta=values[0]-values[1];
        if (delta<0) delta=-delta;
        if (delta<mean*0.15f+5) { result=mean; closest=distance; }
      }
    }
    discharge();
    return result;
  }
};
