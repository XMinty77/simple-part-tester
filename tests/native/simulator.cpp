// Runs the actual .ino with an Arduino GPIO shim. ADC samples are ngspice .op
// solutions, cached by pin state. No transistor recognition is mocked.
#include "Arduino.h"
#include <array>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <unistd.h>
#include <stdexcept>
SerialMock Serial;
static int directions[20]={}, levels[20]={};
void pinMode(uint8_t p,int m) { directions[p]=m; }
void digitalWrite(uint8_t p,int v) { levels[p]=v; }
void delay(unsigned long) {}
static std::string env(const char *key, const char *fallback) {
  const char *v=std::getenv(key); return v?v:fallback;
}
int analogRead(uint8_t pin) {
  std::string device=env("SIM_DEVICE","npn");
  if (device=="gate") {
    unsigned row=0; std::istringstream pins(env("SIM_INPUTS","8 9"));
    int p,i=0; while(pins>>p) row|=(directions[p] && levels[p])<<i++;
    std::string truth=env("SIM_TRUTH","0001");
    if (pin!=std::stoi(env("SIM_OUTPUT","17")) || row>=truth.size()) throw std::runtime_error("bad gate model wiring");
    return truth[row]=='1'?1023:(truth[row]=='0'?0:512);
  }
  static std::map<std::string,std::array<double,3>> cache;
  std::string key;
  for(int i=2;i<20;++i) key+=directions[i]?(levels[i]?'H':'L'):'Z';
  if (!cache.count(key)) {
    std::string stem="/tmp/tester-spice-"+std::to_string(getpid());
    std::ofstream net(stem+".cir");
    net<<"Tester DC model\n";
    int sense[3]={14,15,16}, low[3]={2,4,6}, high[3]={3,5,7};
    std::istringstream mapping(env("SIM_PROBES","14 2 3 15 4 5 16 6 7"));
    for(int i=0;i<3;++i) mapping>>sense[i]>>low[i]>>high[i];
    for(int i=0;i<3;++i) {
      net<<"Rleak"<<i<<" p"<<i<<" 0 1e12\n";
      int ports[3]={sense[i],low[i],high[i]};
      double resistance[3]={25,705,470025};
      for(int j=0;j<3;++j) if(directions[ports[j]]) {
        std::string id=std::to_string(i)+std::to_string(j);
        net<<"V"<<id<<" v"<<id<<" 0 "<<(levels[ports[j]]?5:0)<<"\n";
        net<<"R"<<id<<" v"<<id<<" p"<<i<<" "<<resistance[j]<<"\n";
      }
    }
    int c=0,b=1,e=2;
    std::istringstream order(env("SIM_PINS","0 1 2")); order>>c>>b>>e;
    if(device=="npn" || device=="pnp")
      net<<"Qdut p"<<c<<" p"<<b<<" p"<<e<<" part\n.model part "<<(device=="npn"?"NPN":"PNP")<<" (IS=1e-14 BF="<<env("SIM_BETA","180")<<" BR=4 VAF=100)\n";
    else if(device=="nmos" || device=="pmos") {
      bool n=device=="nmos";
      net<<"Mdut p"<<c<<" p"<<b<<" p"<<e<<" p"<<e<<" part W=100u L=1u\n.model part "<<(n?"NMOS":"PMOS")<<" (LEVEL=1 VTO="<<(n?"1.8":"-1.8")<<" KP=0.002)\n";
      net<<"Dbody p"<<(n?e:c)<<" p"<<(n?c:e)<<" diode\n.model diode D(IS=1e-14)\n";
    } else if(device=="diode")
      net<<"Ddut p"<<c<<" p"<<e<<" part\n.model part D(IS=1e-14)\n";
    else if(device=="resistor" || device=="short")
      net<<"Rdut p"<<c<<" p"<<e<<" "<<(device=="short"?"0.01":env("SIM_OHMS","1000"))<<"\n";
    else if(device!="open") throw std::runtime_error("unknown SIM_DEVICE");
    net<<".control\nset noaskquit\nop\nwrdata "<<stem<<".dat v(p0) v(p1) v(p2)\nquit\n.endc\n.end\n";
    net.close();
    std::string command="ngspice -b "+stem+".cir > "+stem+".log 2>&1";
    if(std::system(command.c_str())!=0) throw std::runtime_error("ngspice failed: "+stem+".log");
    std::ifstream data(stem+".dat"); double scale; std::array<double,3> voltage;
    for(auto &v:voltage) if(!(data>>scale>>v)) throw std::runtime_error("missing SPICE solution: "+stem);
    cache[key]=voltage;
    std::remove((stem+".cir").c_str()); std::remove((stem+".dat").c_str()); std::remove((stem+".log").c_str());
  }
  int sense[3]={14,15,16}, discard;
  std::istringstream mapping(env("SIM_PROBES","14 2 3 15 4 5 16 6 7"));
  for(int i=0;i<3;++i) mapping>>sense[i]>>discard>>discard;
  for(int i=0;i<3;++i) if(sense[i]==pin) return std::max(0,std::min(1023,(int)std::lround(cache[key][i]*1023/5)));
  throw std::runtime_error("unconnected ADC");
}
#ifndef TESTER_SIMAVR
#include "../../firmware/tester/tester.ino"
int main() { try { setup(); while(std::cin.peek()!=EOF) loop(); } catch(const std::exception &e) { std::cerr<<e.what()<<std::endl; return 2; } }

#endif
