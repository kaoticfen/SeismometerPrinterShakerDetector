# Wiring

For a drawn version of all of this — rail layout, per-part detail drawings, and a colour-coded
GPIO map — open [wiring-diagram.html](wiring-diagram.html) in a browser, or read it online at
<https://claude.ai/code/artifact/062ca760-a782-4928-b6bc-2bc52a40bfc3>.

## Bill of materials

| Part | Notes |
|---|---|
| ESP32-WROOM-32 devkit | The classic dual-mode one. An ESP32-S2/S3/C3 will **not** work — no Bluetooth Classic |
| MPU6050 module | GY-521 breakout is the usual one |
| Netum NT-1809 | 58 mm ESC/POS thermal printer |
| 10 kΩ potentiometer | Linear taper |
| Momentary pushbutton | |
| LED + 330 Ω resistor | Any colour |
| Breadboard + jumpers | |

## Connections

```
        ESP32-WROOM-32
       ┌───────────────┐
  3V3 ─┤3V3        GND ├─ GND
       │               │
       │        GPIO21 ├──── SDA ─── MPU6050
       │        GPIO22 ├──── SCL ─── MPU6050
       │               │
       │        GPIO27 ├──── button ──── GND
       │        GPIO26 ├──── 330R ──── LED ──── GND
       │        GPIO34 ├──── pot wiper
       └───────────────┘
```

| Signal | ESP32 pin | Notes |
|---|---|---|
| MPU6050 VCC | 3V3 | The GY-521 has an onboard regulator, so 5V also works |
| MPU6050 GND | GND | |
| MPU6050 SDA | GPIO21 | I²C at 400 kHz |
| MPU6050 SCL | GPIO22 | |
| MPU6050 AD0 | GND | Sets address `0x68`. Most breakouts pull this low already |
| Button | GPIO27 → GND | `INPUT_PULLUP`, so no external resistor |
| LED | GPIO26 → 330 Ω → LED → GND | |
| Pot wiper | GPIO34 | Ends of the pot go to 3V3 and GND |

## Pin constraints worth knowing

**The pot must stay on GPIO32–39.** Those are the ADC1 pins. ADC2 — GPIO0, 2, 4, 12–15, 25–27 — stops working the moment the Wi-Fi driver starts, and it fails *silently*, returning garbage rather than an error. Moving the pot to GPIO25 to tidy up the breadboard would look fine and read nonsense.

GPIO34 is also **input-only** and has no internal pull-up, which is exactly right for a pot but means you can't reuse it for the button.

Other pins to leave alone:

- **GPIO6–11** — wired to the SPI flash. Using them bricks the boot.
- **GPIO0, 2, 12, 15** — strapping pins, read at reset. A pot or LED on these can stop the board booting or force it into download mode.

## Mounting

The accelerometer has to be **rigidly coupled** to whatever you're measuring. Tape or hot-glue the module flat against the desk or floor. A board dangling on its jumper wires measures the resonance of the jumper wires, which is a much larger and much less interesting signal than the one you want.

Orientation: the module's **Z axis** is the seismogram trace, so mount it flat, Z pointing up. X and Y feed the horizontal-peak stat only.

## Power

USB power for the ESP32 is fine. The printer runs off its own internal battery — it does not draw from the ESP32, and the two are connected only over Bluetooth.
