# Firmware Behavior and Technical Notes

This is a code-reading review of `firmware/finalsubbms/finalsubbms.ino`, **not** a record of completed tests.

## Measurement and state logic

1. Three ADC readings (`A0`, `A1`, `A2`) are converted into cumulative tap voltages using a fixed ADC reference and divider ratio.
2. Individual cell voltages are computed as `C1 = tap1`, `C2 = tap2 - tap1`, and `C3 = tap3 - tap2`. The pack voltage is `tap3`.
3. A Beta-model calculation converts an `A3` NTC reading into temperature using the code's supply-voltage and wiring assumptions.
4. If INA219 initializes, measured current is used to classify the charge state. With `INA_CHARGE_SIGN = -1`, a current below `-50 mA` is classified as charging, above `+50 mA` as discharging, otherwise idle.
5. If INA219 is not found, charging-state inference switches to the change in pack voltage. This is only a heuristic and does **not** directly measure current.
6. Each loop checks for overvoltage, undervoltage, or outside-window temperature readings and commands the relay.
7. While the relay is on and the reported state is `CHARGING` with no configured fault, a cell's balancing output is enabled when its voltage exceeds the average by **more than** `0.10 V`. This is a requested digital output, not proof that bleed current actually flows.
8. Measurements and flags are printed over Serial at `115200` baud, with a `300 ms` delay in each loop.

## Known limitations and safety review

The following are important unresolved engineering concerns in the **unmodified supplied code**:

- **Relay asserted at startup:** `setup()` configures the relay output and calls `driveRelay(true)` before the first cell-voltage or temperature fault check. A safe state should be verified and explicitly designed for the real relay module and power circuit.
- **No fault latching or hysteresis:** The relay command immediately follows the latest readings; readings around a threshold could repeatedly toggle it.
- **No guaranteed sensor-failure containment:** ADC/reference errors, disconnected or shorted sense wires, unusual NTC voltages, and invalid measurements are not comprehensively handled as faults.
- **Potential NTC math fault:** `V_TOP_NTC` is set to `3.3 V`, while the ADC conversion uses a `5.0 V` reference. For readings at or above the assumed thermistor supply, the calculation can reach an invalid denominator or resistance. The polarity and supply of the actual thermistor divider must be established.
- **Fixed ADC reference:** Tap-voltage accuracy depends on the real MCU reference voltage, divider accuracy, wiring, and calibration. No calibration records were provided.
- **Balancing implementation is conditional control only:** The code commands digital pins; efficacy and resistor/MOSFET thermal behavior are not confirmed by the source code.
- **Charging-state fallback is not a current sensor:** A change in pack voltage may not reliably distinguish charging from discharging or idle periods.
- **No verification report supplied:** The archive did not contain threshold-trip tests, temperature calibration records, current-sensor checks, relay timing results, or balancing-current measurements.
- **Not a complete battery safety architecture:** Independent overcurrent/short-circuit protection, cell supervision, protective FETs or contactors as appropriate, and robust single-fault behavior require separate design and verification.

Do not describe the project as a validated or certified protective BMS on the basis of the provided code or photographs.

## What was deliberately not changed

The GitHub preparation preserved the main `.ino` file **byte-for-byte**. No safety fixes or behavior changes were applied, because changing hardware-control firmware without a confirmed circuit, selected board, and bench validation would risk presenting an untested revision as the original working implementation.
