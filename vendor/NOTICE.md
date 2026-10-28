# Reference and attribution

The requested starting point is Markus Frejek's [AVR-Transistortester article](https://www.mikrocontroller.net/articles/AVR-Transistortester). The article describes the six terminal permutations, 680 Ω/470 kΩ probing, base-current discrimination and comparison of forward/reverse BJT gain. Its original software notice restricts commercial use.

The maintained source is [Mikrocontroller-net/transistortester](https://github.com/Mikrocontroller-net/transistortester), pinned for this work to commit `1e5674f98f862a9c4e86c0ebb8cd01745c9e88ad`, archive `Software/Markus/ComponentTester-1.45m.tgz` (Markus Reschke, based on Markus Frejek and Karl-Heinz Kübbeler).

`semi-1.45m.c.reference` preserves that archive's `semi.c` (Latin-1 transcoded to UTF-8, otherwise unchanged). Its copyright is (c) 2012–2020 Markus Reschke. `EUPL-v1.1.pdf` is the upstream license. The upstream README explicitly identifies the firmware as EUPL v1.1 and explains the original author's licensing uncertainty. This project is for educational/noncommercial use as requested; that history is preserved here rather than relicensed as MIT.

`firmware/tester/core.h` adapts the probe method and the leakage-corrected common-emitter gain expression in `CheckTransistor`. The GPIO abstraction, restricted recognition rules, Arduino acquisition, protocol, Python host and simulation harness are new work. This is **not** the complete upstream engine or a byte-for-byte port of its detection routine: no inherited calibration, confidence, precision, device coverage or hardware validation is claimed. The original reference is not compiled into the firmware. The complete upstream source remains accessible at the pinned repository revision.

New project code: (c) 2026 Abdelhadi. Distributed under EUPL v1.1; see the accompanying PDF. Upstream attribution and original restrictions remain as stated above.
