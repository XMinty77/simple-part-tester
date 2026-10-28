#!/usr/bin/env bash
set -euo pipefail
# Compile against the real Debian Arduino AVR core, without downloading an IDE.
core_root=${ARDUINO_AVR_ROOT:-/usr/share/arduino/hardware/arduino/avr}
core_dir="$core_root/cores/arduino"
variant_dir="$core_root/variants/standard"
out_dir=build/avr
mkdir -p "$out_dir"
common=(-mmcu=atmega328p -DF_CPU=16000000UL -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR -Os -ffunction-sections -fdata-sections -flto -I"$core_dir" -I"$variant_dir")
cpp=(-std=gnu++11 -fno-exceptions -fno-threadsafe-statics)
for source in "$core_dir"/*.c; do
  avr-gcc "${common[@]}" -c "$source" -o "$out_dir/$(basename "$source").o"
done
for source in "$core_dir"/*.cpp; do
  avr-g++ "${common[@]}" "${cpp[@]}" -c "$source" -o "$out_dir/$(basename "$source").o"
done
for source in "$core_dir"/*.S; do
  avr-gcc "${common[@]}" -x assembler-with-cpp -c "$source" -o "$out_dir/$(basename "$source").o"
done
avr-g++ "${common[@]}" "${cpp[@]}" -Wall -Wextra -Werror -x c++ -c firmware/tester/tester.ino -o "$out_dir/tester.o"
avr-g++ "${common[@]}" -Wl,--gc-sections "$out_dir"/*.o -lm -o "$out_dir/tester.elf"
avr-objcopy -O ihex -R .eeprom "$out_dir/tester.elf" "$out_dir/tester.hex"
avr-size -C --mcu=atmega328p "$out_dir/tester.elf"
