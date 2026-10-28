// Execute the real AVR ELF, route UART commands, and supply ngspice ADC values.
#define TESTER_SIMAVR
#include "simulator.cpp"
extern "C" {
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/avr_uart.h>
#include <simavr/avr_adc.h>
#include <simavr/avr_ioport.h>
}
static avr_t *mcu;
static unsigned lines;
static void output(avr_irq_t *, uint32_t value, void *) {
  std::cout<<static_cast<char>(value);
  if (value=='\n') { ++lines; std::cout.flush(); }
}
static void sample(avr_irq_t *, uint32_t, void *) {
  avr_ioport_state_t ports[3]={};
  const char names[3]={'D','B','C'};
  for (int i=0;i<3;++i) avr_ioctl(mcu,AVR_IOCTL_IOPORT_GETSTATE(names[i]),&ports[i]);
  for (int pin=2;pin<20;++pin) {
    int index=pin<8?0:(pin<14?1:2), bit=pin<8?pin:(pin<14?pin-8:pin-14);
    directions[pin]=(ports[index].ddr>>bit)&1;
    levels[pin]=(ports[index].port>>bit)&1;
  }
  unsigned channel=mcu->data[0x7c]&15; // ATmega328P ADMUX
  if (channel>5) throw std::runtime_error("unexpected ADC channel");
  unsigned mv=std::lround(analogRead(14+channel)*5000.0/1023);
  avr_raise_irq(avr_io_getirq(mcu,AVR_IOCTL_ADC_GETIRQ,channel),mv);
}
static void step() {
  int state=avr_run(mcu);
  if(state==cpu_Done || state==cpu_Crashed) throw std::runtime_error("AVR stopped");
}
static void released() {
  for(char name: {'B','C','D'}) {
    avr_ioport_state_t port={};
    avr_ioctl(mcu,AVR_IOCTL_IOPORT_GETSTATE(name),&port);
    unsigned mask=name=='D'?0xfc:0x3f;
    if((port.ddr&mask) || (port.port&mask)) throw std::runtime_error("GPIO not released after command");
  }
}
int main(int argc,char **argv) {
  try {
    if(argc!=2) throw std::runtime_error("usage: tester-avr-sim firmware.elf");
    elf_firmware_t firmware={};
    if(elf_read_firmware(argv[1],&firmware)) throw std::runtime_error("cannot read ELF");
    mcu=avr_make_mcu_by_name("atmega328p");
    avr_init(mcu);
    mcu->frequency=16000000;
    mcu->vcc=mcu->avcc=mcu->aref=5000;
    avr_load_firmware(mcu,&firmware);
    uint32_t flags=0;
    avr_ioctl(mcu,AVR_IOCTL_UART_SET_FLAGS('0'),&flags);
    avr_irq_register_notify(avr_io_getirq(mcu,AVR_IOCTL_UART_GETIRQ('0'),UART_IRQ_OUTPUT),output,nullptr);
    avr_irq_register_notify(avr_io_getirq(mcu,AVR_IOCTL_ADC_GETIRQ,ADC_IRQ_OUT_TRIGGER),sample,nullptr);
    while(mcu->cycle<100000) step();
    std::string command;
    while(std::getline(std::cin,command)) {
      unsigned target=lines+1;
      command+='\n';
      for(unsigned char ch:command) {
        avr_raise_irq(avr_io_getirq(mcu,AVR_IOCTL_UART_GETIRQ('0'),UART_IRQ_INPUT),ch);
        auto end=mcu->cycle+2000;
        while(mcu->cycle<end) step();
      }
      auto deadline=mcu->cycle+16000000ULL*30;
      while(lines<target && mcu->cycle<deadline) step();
      if(lines<target) throw std::runtime_error("AVR response timed out");
      // Complete the trailing Serial transmission and release code.
      auto end=mcu->cycle+10000;
      while(mcu->cycle<end) step();
      released();
    }
    avr_terminate(mcu);
  } catch(const std::exception &e) { std::cerr<<e.what()<<std::endl; return 2; }
}
