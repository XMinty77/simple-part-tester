.PHONY: test native avr clean
native: build/tester-native
build/tester-native: firmware/tester/tester.ino firmware/tester/core.h tests/native/Arduino.h tests/native/simulator.cpp
	mkdir -p build
	g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itests/native tests/native/simulator.cpp -o $@
test: native
	python3 -m unittest discover -s tests -v
avr:
	bash scripts/build-avr.sh
clean:
	rm -rf build

.PHONY: simavr
build/tester-avr-sim: tests/native/avr_simulator.cpp tests/native/simulator.cpp tests/native/Arduino.h
	mkdir -p build
	g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itests/native $< -lsimavr -o $@
simavr: avr build/tester-avr-sim
	python3 tests/check_avr.py
