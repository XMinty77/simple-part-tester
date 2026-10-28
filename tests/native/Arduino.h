#pragma once
#include <stdint.h>
#include <iostream>
#include <iomanip>
#define TESTER_NATIVE 1
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1
#define DEFAULT 1
class __FlashStringHelper;
#define F(s) reinterpret_cast<const __FlashStringHelper *>(s)
void pinMode(uint8_t,int);
void digitalWrite(uint8_t,int);
int analogRead(uint8_t);
void delay(unsigned long);
inline void analogReference(int) {}
struct SerialMock {
  void begin(int) {}
  int available() { return std::cin.peek()!=EOF; }
  int read() { return std::cin.get(); }
  void print(const __FlashStringHelper *s) { std::cout<<reinterpret_cast<const char *>(s); }
  void print(const char *s) { std::cout<<s; }
  void print(char c) { std::cout<<c; }
  void print(unsigned char n) { std::cout<<static_cast<unsigned>(n); }
  void print(int n) { std::cout<<n; }
  void print(float f, int digits) { std::cout<<std::fixed<<std::setprecision(digits)<<f; }
  template<typename T> void println(T value) { print(value); std::cout<<std::endl; }
};
extern SerialMock Serial;
