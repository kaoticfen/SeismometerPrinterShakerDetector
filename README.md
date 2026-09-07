# SeismometerPrinterShakerDetector

A desk seismograph. An ESP32 watches an accelerometer, and when you press a
button it prints the last 30 seconds of building vibration on a thermal printer
as a proper seismogram — footsteps in the corridor, someone jiggling their leg,
the goods lift, whatever the floor is doing.

Time runs down the paper at 2.5 mm/s, oldest at the top, the way a drum
recorder does it.

```
 0 ─         ╱╲                        record #7  t+0h14m22s
   ─      ───╫───                      window   30.0 s
   ─         ╲╱                        fullscale +/-150.0 mg
 5 ─       ──╫──                       peak     125.1 mg
   ─    ─────╫─────                    rms      14.93 mg
10 ─  ───────╫───────                  dom freq 1.83 Hz
```

## Hardware

ESP32-WROOM-32, MPU6050, Netum NT-1809 thermal printer, plus a button, an LED
and a pot on a breadboard. Full parts list and pinout in
[docs/WIRING.md](docs/WIRING.md).

The ESP32 must be a **classic ESP32-WROOM**, not an S2/S3/C3 — the printer link
needs Bluetooth Classic, which those don't have.

## Build

```sh
python3 -m venv .venv
.venv/bin/pip install platformio
```

### 1. Find the printer

The NT-1809's datasheet says "Bluetooth 4.0" and it supports iOS, which means
it's dual-mode — but nothing published says whether it exposes Classic SPP,
BLE, or both. So find out rather than guess. Power-cycle the printer so it's
discoverable, then:

```sh
.venv/bin/pio run -e probe -t upload -t monitor
```

It scans both radios and prints a ready-made `PRINTER_MAC` line. Paste that into
[src/config.h](src/config.h), and set `PRINTER_USE_BLE` to `0` if it turned up
under Classic or `1` if only under BLE.

### 2. Flash the firmware

```sh
.venv/bin/pio run -e seismo -t upload -t monitor
```

## Using it

| Control | Does |
|---|---|
| Short press | Print the last 30 seconds |
| Long press (>1 s) | Print a test page |
| Pot | Graph full scale, 5 mg to 500 mg, log taper |
| LED | Lit while the signal is over half full scale |

There's also a live view: join the Wi-Fi network **SEISMO** (password
`shakeitup`) and open <http://192.168.4.1>. It shows a scrolling trace, the
current peak/RMS/dominant frequency, and a print button. Use it to set the pot
before you spend paper — bins that would clip on the printout show up red.

**Print the test page first.** It exercises the whole Bluetooth and ESC/POS path
without involving the sensor, so if it fails you know the problem is the link
rather than the DSP.

## How it works

- **Sampling** — 200 Hz off the MPU6050 at ±2 g, from a task pinned to core 1 so
  the radios on core 0 can't jitter the sample clock. The on-chip low-pass sits
  at 21 Hz.
- **Filtering** — a 0.5 Hz one-pole high-pass per axis strips gravity and
  thermal drift. The high-passed Z axis is the trace.
- **Buffering** — a 30 s ring of `int16` counts, statically allocated, because
  heap on a chip running both Wi-Fi and Bluetooth is not somewhere to keep 24 KB.
- **Decimation** — each printed row is the **min and max** of the 10 samples in
  its time bin, drawn as a vertical bar. Averaging instead would flatten
  footsteps to nothing: a sharp bipolar impulse averages to about zero. Min/max
  is what drum recorders and audio waveform views do, for the same reason.
- **Printing** — ESC/POS `GS v 0` raster, 384 dots wide, sent in 24-row chunks
  with a pause between them.

## Troubleshooting

**Rows missing or the graph looks shredded.** The printer's input buffer
overran. Neither SPP nor BLE gives real flow control, so the pacing is manual —
raise `RASTER_CHUNK_PAD_MS` in [src/config.h](src/config.h).

**Graph is grey and patchy rather than black.** Heat settings. Raise the second
byte of the `ESC 7` sequence in `Printer::init()`.

**Trace is noisy even when nothing is moving.** Almost always mounting. The
module needs to be stuck down flat; on jumper wires it measures the wires. At
rest on a solid surface you should see a flat line within about ±2 mg.

**Full scale reads nonsense or won't move.** The pot is on an ADC2 pin. It must
be on GPIO32–39 — ADC2 is dead whenever Wi-Fi is running, and it fails silently.

**`SPP connect failed` on repeat.** The printer drops its radio when idle, so
occasional failures are normal and it retries with backoff. Persistent failure
usually means it's still paired to a phone — unpair it there first.

**Sensor never reads.** Check `WHO_AM_I` in the serial log. No response at all
means SDA/SCL are swapped or AD0 is floating. A clone reporting `0x70`/`0x98`
instead of `0x68` is fine and works normally.

## Layout

```
src/
  config.h       pins, rates, layout constants, printer MAC
  main.cpp       button/LED/pot, print orchestration
  Sensor.*       MPU6050, 200 Hz task, ring buffers, locked snapshots
  Filters.h      one-pole high-pass
  RingBuffer.h   fixed-capacity static ring
  Waveform.*     min/max decimation, peak/RMS, FFT dominant frequency
  Bitmap.*       384-dot raster, 5x7 digit font, grid and trace drawing
  Printer.*      ESC/POS, chunked raster, seismogram and test page
  BtLink.*       SPP transport, BLE fallback, reconnect backoff
  WebUI.*        SoftAP, JSON API
  web_index.h    the live-view page
tools/bt_probe/  discovery firmware, run this first
```
