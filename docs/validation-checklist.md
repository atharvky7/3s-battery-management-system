# Validation Checklist (Not Yet Documented)

The original uploaded ZIP contains source code, photographs, and a circuit sketch, but no quantitative results. All entries below are **unverified** until the project team supplies measured observations.

| Check | Status | Evidence to add |
| --- | --- | --- |
| Arduino board and wiring confirmed | Not documented | Board model, pin-by-pin cross-check |
| Library dependencies and clean compilation | Not documented | Board selection, package versions, build output |
| Cell 1/2/3 and pack voltage accuracy | Not documented | Reference meter readings and observed serial readings |
| NTC temperature calculation checked | Not documented | Sensor orientation, supply, reference, two or more comparison points |
| INA219 current polarity and readings | Not documented | Reference readings and charge/discharge sign |
| Relay defaults to a safe state at power-on | **Needs design review** | Bench observations with safe isolated supply |
| Relay cutoff at voltage/temperature limits | Not documented | Threshold tests in a controlled simulator setup |
| Sensor-disconnect/error behavior | Not documented | Fault-injection results without live battery pack |
| Balancing hardware and thermal behavior | Not documented | Resistor and transistor values, current, temperature |
| Independent protections and system safety | Not documented | Protection architecture and verified limits |

Once real results are available, replace the corresponding status with **Validated**, **Failed**, or **Partially validated**, accompanied by dates, conditions, numerical values, and evidence. Do not mark a check as passed solely because the firmware contains the relevant branch.
