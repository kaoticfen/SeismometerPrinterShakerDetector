# Wiring

For a drawn version of all of this — rail layout, per-part detail drawings, and a colour-coded
GPIO map — open [wiring-diagram.html](wiring-diagram.html) in a browser, or read it online at
<https://claude.ai/code/artifact/062ca760-a782-4928-b6bc-2bc52a40bfc3>. It draws the
full-scale control as a rotary pot; a trimmer is the same three-terminal divider, so
every connection there still applies.

## Bill of materials

| Part | Notes |
|---|---|
| ESP32-WROOM-32 devkit | DOIT ESP32 DEVKIT V1, 30-pin — the board this doc's pin map matches. Any ESP32-WROOM-32 works; an ESP32-S2/S3/C3 will **not** — no Bluetooth Classic |
| MPU6050 module | GY-521 breakout is the usual one |
| Netum NT-1809 | 58 mm ESC/POS thermal printer |
| 10 kΩ trimmer potentiometer | Single-turn 3362P or 3296W, or any 0.1"-pitch trimmer. Linear (code `103`) |
| Momentary pushbutton | 4-pin tact switch |
| LED + 330 Ω resistor | Any colour |
| Breadboard (830-point) + jumpers | |

## Connections

Pin names below are the ones **printed on the board**. On the DOIT V1 the silkscreen
says `D34`, not `GPIO34` — `src/config.h` uses the GPIO numbers, and for the `D` pins
the two are the same number.

```
        ESP32 DEVKIT V1
       ┌───────────────┐
  3V3 ─┤3V3        GND ├─ GND
       │               │
       │           D21 ├──── SDA ─── MPU6050
       │           D22 ├──── SCL ─── MPU6050
       │               │
       │           D27 ├──── button ──── GND
       │           D26 ├──── 330R ──── LED ──── GND
       │           D34 ├──── trimmer wiper
       └───────────────┘
```

| Signal | Board label | GPIO | Notes |
|---|---|---|---|
| MPU6050 VCC | `3V3` | — | Bottom row, USB end. The GY-521 has an onboard regulator, so 5V also works |
| MPU6050 GND | `GND` | — | Either row has one |
| MPU6050 SDA | `D21` | GPIO21 | I²C at 400 kHz |
| MPU6050 SCL | `D22` | GPIO22 | |
| MPU6050 AD0 | `GND` | — | Sets address `0x68`. Most breakouts pull this low already |
| Button | `D27` → `GND` | GPIO27 | `INPUT_PULLUP`, so no external resistor |
| LED | `D26` → 330 Ω → LED → `GND` | GPIO26 | |
| Trimmer wiper | `D34` | GPIO34 | Centre pin. The two outer pins go to 3V3 and GND |

## The ESP32 devkit on the breadboard

The DOIT ESP32 DEVKIT V1, 30 pins, laid out as in the build photo: **board lying
across the breadboard, USB socket to the left.** Pins this build uses are marked
`▲`.

```
     VIN GND D13 D12 D14 D27 D26 D25 D33 D32 D35 D34  VN  VP  EN
      ╵   ╵   ╵   ╵   ╵   ▲   ▲   ╵   ╵   ╵   ╵   ▲   ╵   ╵   ╵
    ┌─┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴─┐
    │ ○  [EN]                                                ○ │
   ▭│                     ESP-32  WROOM-32                     │
 USB │ [BOOT]                                                   │
    │ ○                                                      ○ │
    └─┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬─┘
      ▲   ▲   ╷   ╷   ╷   ╷   ╷   ╷   ╷   ╷   ▲   ╷   ╷   ▲   ╷
     3V3 GND D15  D2  D4 RX2 TX2  D5 D18 D19 D21 RX0 TX0 D22 D23
```

Board label → GPIO, for the pins whose names don't say it:

| Label | GPIO | | Label | GPIO |
|---|---|---|---|---|
| `VP` | GPIO36 | | `RX2` | GPIO16 |
| `VN` | GPIO39 | | `TX2` | GPIO17 |
| `TX0` | GPIO1 | | `RX0` | GPIO3 |

Every `Dnn` label is that GPIO number — `D34` is GPIO34. `VIN` and `EN` are not GPIOs.

**`VIN` is 5 V, and it sits directly across from `3V3`.** Both are the first pin of
their row at the USB end, so they are the two easiest pins on the board to mix up.
Feed the + rail from `VIN` and the trimmer puts 5 V on `D34` — that pin is not 5 V
tolerant. The 3V3 pin is on the **bottom** row in the orientation above; in the
photo it's the one with the red jumper.

**The build needs both rows**, so the devkit has to straddle the centre channel:
`D34`, `D27`, `D26` are on the top row, and `3V3`, `GND`, `D21`, `D22` on the bottom.
The board is 0.9" wide, which covers seven columns of an 830-point breadboard and
leaves **one free column on each side** — one jumper per pin, which is all this build
needs. There is no way to reach the top row with the board pushed to one half.

Seat it at one end of the board so the USB cable clears the parts, and keep the rows
opposite `D21`/`D22` free for the MPU6050's jumpers.

If you swap in a 38-pin DevKitC later, the same five signals are there but the flash
pins (GPIO6–11) appear on the header and the row order shifts — go by the silkscreen,
not by counting positions.

## Breadboard layout

Both power rails first, then everything hangs off them:

| Rail | Fed from |
|---|---|
| **+ rail** | ESP32 `3V3` — bottom row, USB end. **Not `VIN`** |
| **− rail** | ESP32 `GND` — the pin next to `3V3` |

Both rail-feed jumpers come off the same two pins at the USB end of the bottom row,
which is the red-and-black pair in the build photo.

Then, part by part:

```
  + rail  ═══════════════════════════════════════════════════
             │                    │              │
             │              trimmer pin 1    MPU VCC
             │
             │   trimmer pin 2 (wiper) ────────── D34
             │
             │              trimmer pin 3    MPU AD0 ─┐
             │                    │                   │
  − rail  ═══╧════════════════════╧═══════════════════╧══════
                    │         │            │
                 MPU GND   button      LED cathode
                              │            │
                             D27       330 Ω ── D26

  MPU SDA ── D21             MPU SCL ── D22
```

**The trimmer** — the blue 3386 in the photo — has its three pins in a line on 0.1"
pitch, so like the MPU6050 it goes into **three consecutive rows of one column
half**, one pin per row. No channel-straddling.

Getting this axis backwards is the single fatal trimmer mistake: three pins pushed
into three adjacent *columns of the same row* — `38a`, `38b`, `38c` — are all one
node, so the whole part is a short from + rail to − rail with `D34` welded to the
middle of it. The pins must climb the row numbers, not the column letters.

Counting from the trimmer's pin 1 in row *N*:

| Row | Pin | Goes to |
|---|---|---|
| *N* | outer | + rail |
| *N*+1 | **wiper** | `D34` |
| *N*+2 | outer | − rail |

**The two outer pins are interchangeable.** The track is symmetric, so neither end
is inherently the + or the − one — the choice only decides which way the screw
turns to increase full scale. If it feels backwards, swap the two rail wires. A
wrong guess is harmless: 10 kΩ across 3.3 V is 0.33 mA.

Only the wiper's identity matters. Turn the screw end to end with `pio run -e
seismo -t monitor` and watch the full-scale figure from [main.cpp](../src/main.cpp):

| Serial output | Meaning |
|---|---|
| Sweeps 5 → 500 mg | Correct |
| Sweeps backwards | Fine — swap the rail wires if you care |
| Pinned at 5 mg, screw does nothing | Wiper swapped with an outer pin. No current flows through the `D34` leg, so it sits at the wiper's potential — ground |
| Drifts when a hand is near | Only one outer pin is landing; the divider is incomplete |

**Confirm the wiper with a meter rather than trusting the pin order.** Across the
two outer pins you read a fixed 10 kΩ at any screw position; from either outer pin
to the wiper the reading sweeps as you turn. On an in-line 3386P or 3362P the wiper
is the centre pin, but some trimmer bodies put the pins in a triangle and the odd
one out is the wiper.

Feed the top of the divider from **+ 3V3, never `VIN`** — at 5 V the wiper puts 5 V
on `D34`, which is not 5 V tolerant.

**The button** must straddle the centre channel — see [Two breadboard
gotchas](#two-breadboard-gotchas). In this build its four legs are in **21e, 21f,
23e, 23f**:

```
        a  b  c  d  e │ f  g  h  i  j
row 21  ●──────────[leg]│[leg]──●───────
                    ╎  │  ╎            ← the switch's own internal bridges,
row 23  ───────────[leg]│[leg]───────────  one down each side, always closed
        ↑                 ↑
      − rail            D27
```

| Wire | From | To |
|---|---|---|
| Button → ground | − rail | `21a` |
| Button → ESP32 | `21g` | `D27` |

Either free hole on a side works — `21a` through `21e` are one strip, as are `21f`
through `21j`, so `21g` is the same node as the leg in `21f`.

**Leave row 23 empty.** The switch bridges 21 to 23 down each side whether or not
it's pressed, so anything landed in row 23 is wired straight onto the button's
nodes. A + rail wire there shorts the supply through the − rail wire in row 21.

There is **no + rail wire on the button** — the pull-up is internal to the ESP32.

**The MPU6050** does *not* straddle the centre channel — the GY-521 has a single
8-pin header along one edge, so all eight pins go into **eight consecutive rows of
one column half**, and each pin lands on its own isolated row. The body overhangs
the channel; only the pins matter. Counting from `VCC` in row *N*:

| Row | Pin | Goes to |
|---|---|---|
| *N* | `VCC` | + rail |
| *N*+1 | `GND` | − rail |
| *N*+2 | `SCL` | `D22` |
| *N*+3 | `SDA` | `D21` |
| *N*+4 | `XDA` | — leave empty |
| *N*+5 | `XCL` | — leave empty |
| *N*+6 | `ADO` | − rail |
| *N*+7 | `INT` | — leave empty |

Each jumper goes in the **same numbered row as its pin, same half** — a free hole
in `a`–`e` if the header is in `a`–`e`, since those five holes are one node.

**`SCL` and `SDA` cross over.** The module lists `SCL` above `SDA`; the ESP32
header has `D21` before `D22`. Wiring them straight across — module `SCL` to the
nearer `D21` — swaps the bus and `Sensor::begin()` fails its `WHO_AM_I` read at
boot with nothing else to show for it. `SDA`→`D21`, `SCL`→`D22`, and the two wires
visibly cross.

`XDA` and `XCL` are the auxiliary I²C master for a magnetometer this build doesn't
have, and `INT` is unused — [Sensor.cpp](../src/Sensor.cpp) polls. Leave all three
in air; grounding `XDA`/`XCL` does nothing good.

## Two breadboard gotchas

**The trimmer needs all three legs.** It only reads as a position when it is a
*divider*: 3V3 on one outer pin, GND on the other, wiper to `D34`. Wire just the
wiper and one end and `D34` is a floating input behind a resistor — it has no
internal pull-down, so it drifts with whatever your hand is near.

**A tact switch's four pins are two pairs, joined inside the body.** The two pins
on the same side of the switch are permanently connected. Land both jumpers on one
joined pair and `D27` sits at ground forever — the firmware reads a button held
down since boot and prints on its own. Straddling the centre channel puts the
jumpers on opposite pairs and the geometry sorts itself out.

The two dimensions of a 6×6 mm tact switch are what make this work, and they are
easy to get backwards. **The joined legs are the 4.5 mm pair** — the two on the
same side — which is 2 breadboard holes. The gap between the two sides is 6.5 mm,
which is 3 holes, and 3 holes is exactly the width of the centre channel. So the
switch only ever wants one orientation: the 6.5 mm axis across the channel, the
4.5 mm axis along the rows. Turn it 90° and both joined pairs land inside the same
`a`–`e` half, where the breadboard's own row strips short them into one node and
the switch does nothing at all.

Before powering up, check continuity between the two jumper holes: **open at rest,
closed only while the button is held.**

## Pin constraints worth knowing

**The trimmer must stay on `D32`–`D39`.** Those are the ADC1 pins — on this board that's `D32`, `D33`, `D34`, `D35`, `VP` and `VN`, all clustered at the far end of the top row. ADC2 — `D0`, `D2`, `D4`, `D12`–`D15`, `D25`–`D27` — stops working the moment the Wi-Fi driver starts, and it fails *silently*, returning garbage rather than an error. Moving the wiper one pin over to `D25` to tidy up the breadboard would look fine and read nonsense.

`D34` is also **input-only** and has no internal pull-up, which is exactly right for a pot but means you can't reuse it for the button. Same goes for its neighbours `D35`, `VP` and `VN`.

Other pins to leave alone:

- **`D2`, `D12`, `D15`** — strapping pins, read at reset. A trimmer or LED on these can stop the board booting or force it into download mode. GPIO0 is the fourth, but the DOIT V1 doesn't break it out — it goes to the `BOOT` button instead.
- **`TX0`/`RX0`** — the USB serial console. Using them breaks `pio monitor`.
- **GPIO6–11** — the SPI flash. Using them bricks the boot. The DOIT V1 doesn't break them out, so you'd have to solder to reach them; on a 38-pin DevKitC they're right there on the header.

## Mounting

The accelerometer has to be **rigidly coupled** to whatever you're measuring. Tape or hot-glue the module flat against the desk or floor. A board dangling on its jumper wires measures the resonance of the jumper wires, which is a much larger and much less interesting signal than the one you want.

Orientation: the module's **Z axis** is the seismogram trace, so mount it flat, Z pointing up. X and Y feed the horizontal-peak stat only.

## Power

USB power for the ESP32 is fine. The printer runs off its own internal battery — it does not draw from the ESP32, and the two are connected only over Bluetooth.
