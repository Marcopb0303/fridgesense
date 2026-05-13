# FridgeSense — Analog Food Safety Monitor

A battery-friendly food spoilage monitor built on a Teensy microcontroller and a discrete 6-bit R-2R resistor-ladder DAC. The DAC output encodes a continuous analog food safety index (0–2.2 V) that drives both an LED brightness indicator and a speaker with distinct tone signatures per alert zone.

## How it works

A potentiometer (Phase 1) or real humidity/VOC sensors (Phase 2) feed an analog reading into the Teensy ADC. Firmware maps that reading through a food-type profile to a 6-bit DAC code, which the R-2R ladder converts to a proportional analog voltage. That voltage simultaneously:

- Sets LED brightness via PWM — brighter = higher risk
- Triggers a zone-specific speaker tone pattern when thresholds are crossed
- Appears on the DAC output node as a measurable analog signal (verifiable with an oscilloscope)

## Safety zones

| Zone | DAC code | Voltage | Tone signature |
|------|----------|---------|----------------|
| SAFE | 0–20 | 0.00–0.69 V | Silence |
| MONITOR | 21–38 | 0.72–1.31 V | Single soft tone every 10 s |
| WARNING | 39–52 | 1.34–1.79 V | Rising two-tone every 5 s |
| ALERT | 53–63 | 1.82–2.17 V | Rapid triple pulse every 2 s |

## Food profiles

A button cycles through four profiles. Each profile applies a different sensitivity scaling so the same input level reaches warning sooner for high-risk foods:

| Profile | Sensitivity | Max days (Phase 2) |
|---------|-------------|-------------------|
| Fruits | 1.2× | 7 days |
| Vegetables | 1.4× | 5 days |
| Dairy | 1.0× | 14 days |
| Leftovers | 1.6× | 3 days |

## Hardware

| Component | Pin |
|-----------|-----|
| R-2R DAC bit 0 (MSB) | 14 |
| R-2R DAC bit 1 | 15 |
| R-2R DAC bit 2 | 16 |
| R-2R DAC bit 3 | 17 |
| R-2R DAC bit 4 | 18 |
| R-2R DAC bit 5 (LSB) | 19 |
| Potentiometer input | A8 |
| LED (PWM) | 21 |
| Speaker | 20 |
| Profile button | 22 |

**R-2R ladder:** 1 kΩ (R) and 2 kΩ (2R) resistors, 6-bit, 64 output levels, Vmax ≈ 2.17 V from 3.3 V supply.

## Repository structure

```
fridgesense/
├── README.md
├── phase1/
│   └── fridgesense_phase1.ino   ← Teensy firmware, pot input, DAC + speaker + LED output
└── docs/
    └── scope_captures/          ← oscilloscope captures of DAC staircase and tone waveforms
```

## Roadmap — Phase 2

- Replace potentiometer with DHT22 (humidity) + ENS160 (VOC/eCO2) sensors
- Add elapsed-time tracking per food item entered via serial
- Integrate ESP32-C3 for WiFi push notifications to phone:  
  *"5 days since purchase — VOC rising in vegetable section. Check your fridge."*
- Two-microcontroller split: Teensy handles analog output, ESP32-C3 handles sensing and communication

## Dependencies

- [Teensyduino](https://www.pjrc.com/teensy/teensyduino.html) — Arduino core for Teensy 4.0
- No external libraries required for Phase 1
