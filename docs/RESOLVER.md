# C2000 resolver frequency windows — 5.16.R

The former PWM ISR used a fixed 1220-cycle divider and treated every window as 100 ms. At 6103 Hz PWM, a simulated 10 Hz rotor was reported as 19.96875 Hz. At 24414 Hz PWM, the same divider produced approximately half the actual rotor frequency.

## Behaviour

The PWM ISR calls `Encoder::UpdateRotorFrequency()` once per ADC interrupt, including when motor control is off. The encoder closes a window after `ceil(pwmFrequency / 10)` cycles and computes the reporting rate as `pwmFrequency / elapsedCycles`. This uses the integer PWM frequency returned by the timer driver, including the small rounding difference from an exact 100 ms interval.

The three supported frequencies, 6103, 12207, and 24414 Hz, therefore use 611, 1221, and 2442 cycles respectively. A return value of `true` tells the ISR to refresh the `speed` parameter. The angle decoder, amplitude threshold, 4000-cycle startup delay, 500 Hz candidate rejection, and 200 Hz reported-frequency cap retain their existing behaviour.

PWM reconfiguration discards a partial window and clears the encoder's previous frequency and diagnostic accumulators while preserving the previous interrupt mask. A zero or unsupported frequency disables window reporting. Windows with no angle samples report zero frequency and direction, avoiding stale speed after motor control stops.

## Host regression check

`C2000ResolverTest` compiles the production C2000 `encoder.cpp`. Only the ADC sample provider and interrupt-mask boundary are replaced for the host; the frequency calculation is not duplicated in a test implementation. Release builds keep assertions enabled.

With the normal CMake dependencies installed:

```sh
cmake -S . -B build/host-sine -G "Unix Makefiles" -DCONTROL=SINE -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-sine --target OpenInverterTest HvilInterlockTest C2000ResolverTest
ctest --test-dir build/host-sine/test --output-on-failure
```

Repeat with a separate `build/host-foc` directory and `-DCONTROL=FOC`. macOS host compatibility options are described in [HVIL validation](HVIL.md). CI runs the registered host checks through CTest.

The resolver check covers both directions at every supported PWM rate, window boundaries, a partial-window frequency change, zero/unsupported configuration, OFF-mode windows, diagnostic reset, and preservation of an already-disabled interrupt mask. Simulated 10 Hz and 100 Hz inputs must remain within 0.0625 Hz of the reference. With two pole pairs, their speed checks expect 300 and 3000 RPM respectively, within 2 RPM.

## Software validation — 2026-10-05

Host CTest passed 36/36 checks in SINE and 45/45 in FOC. The resolver executable also passed a strict C++98 build with release assertions, AddressSanitizer, and UndefinedBehaviorSanitizer. A temporary version assuming an exact 10 Hz reporting rate failed the accuracy assertion. Both C2000 `inverter` RAM images compiled and linked with TI 25.11.1.LTS on macOS arm64; no hardware was loaded or exercised.

## Bench acceptance — pending

Software checks do not establish ADC timing, ISR execution cost, resolver analogue quality, or speed accuracy on an inverter. Use RAM-only loading before making a hardware-validation claim.

1. Compare `speed` with an independent speed measurement at all three PWM rates in both directions.
2. Check resolver-window sample counts and reporting intervals while changing PWM configuration.
3. Measure ISR execution time and ADC overflow counts under load; this change does not compensate for missed ADC interrupts.
