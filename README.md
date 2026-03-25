# Orange Pi PC Plus — Fan Control

*Temperature-based GPIO fan controller for Allwinner H3*

---

## Overview

This program monitors the CPU temperature of the Orange Pi PC Plus and controls a 5V fan via a GPIO pin using a transistor circuit. The fan turns on when the CPU exceeds 70°C and turns off when it drops below 65°C. The 5°C hysteresis prevents rapid on/off cycling.

| Parameter | Value |
|---|---|
| GPIO Pin | Physical pin 7 → PA6 → Linux GPIO 6 |
| Fan ON threshold | 70°C |
| Fan OFF threshold | 65°C |
| Poll interval | 5 seconds |
| Logic level | Active-low (0 = fan ON, 1 = fan OFF) |

---

## Hardware Wiring

The fan is driven by a transistor switch controlled by the GPIO pin. You need:
- NPN or PNP transistor (e.g. 2N2222 / BC547 for NPN, BC557 / 2N3906 for PNP)
- 1kΩ base resistor
- 1N4007 flyback diode
- 5V fan

### NPN Transistor (Active-High Logic)

With NPN, GPIO HIGH (1) = fan ON.

```
GPIO Pin 7 (PA6)
      |
     [1kΩ]        ← base resistor
      |
 Base (B) ──── NPN transistor (e.g. 2N2222)
                  |
            Collector (C)
                  |
             [Fan –]  ← negative fan wire
                  |
             [Diode]  ← 1N4007, cathode toward +5V
                  |
             [Fan +]  ← positive fan wire
                  |
              VCC-5V  ← Pin 2 or 4 on header
                  |
            Emitter (E)
                  |
                 GND  ← Pin 6, 9, 14... on header
```

> **Note:** With NPN use `echo 1` to turn fan ON, `echo 0` to turn it OFF. Update the `GPIO_VALUE` writes in the C code accordingly if using this variant.

### PNP Transistor (Active-Low Logic — used in this code)

With PNP, GPIO LOW (0) = fan ON. The code is written for this variant.

```
VCC-5V  (Pin 2 or 4)
      |
 Emitter (E)
                  |
 Base (B) ──── PNP transistor (e.g. BC557)
      |
     [1kΩ]        ← base resistor
      |
 GPIO Pin 7 (PA6) ← pulled LOW to activate
                  |
            Collector (C)
                  |
             [Fan +]  ← positive fan wire
                  |
             [Diode]  ← 1N4007, cathode toward fan+
                  |
             [Fan –]
                  |
                 GND  ← any GND pin on header
```

> **Warning:** The flyback diode is critical. The fan motor generates a voltage spike when switched off. Without the diode this spike can damage the transistor or the GPIO pin.

### Flyback Diode Placement

- **Cathode (stripe end)** always faces toward the positive supply (+5V side of fan)
- **Anode** faces toward GND
- The diode is placed in **parallel** with the fan, not in series

### Connecting to the Board

| Signal | Pin # | Header Name | Notes |
|---|---|---|---|
| GPIO control | 7 | PWM1 (PA6) | Linux GPIO 6 |
| 5V power | 2 or 4 | VCC-5V | Fan supply |
| Ground | 6, 9, 14, 20… | GND | Common ground |

> **Warning:** Never power the fan directly from the GPIO pin. GPIO pins on the H3 are 3.3V and can only source ~10mA. Always use a transistor to switch the fan from the 5V rail.

---

## Compiling

The Orange Pi PC Plus uses an Allwinner H3 (ARM Cortex-A7, 32-bit). Cross-compile from macOS using Docker.

**Prerequisites:** Docker Desktop for Mac and your source saved as `fan_control.c`.

Run this from the directory containing `fan_control.c`:

```bash
docker run --rm -v $(pwd):/work -w /work debian:bookworm bash -c \
  "apt-get update -q && apt-get install -y -q gcc-arm-linux-gnueabihf && \
   arm-linux-gnueabihf-gcc -O2 -o fan_control fan_control.c"
```

This produces a `fan_control` binary ready to run on the Orange Pi.

**Verify the binary (optional):**

```bash
docker run --rm -v $(pwd):/work -w /work debian:bookworm bash -c \
  "apt-get install -y -q file && file fan_control"
```

Expected: `ELF 32-bit LSB executable, ARM, EABI5 — dynamically linked, for GNU/Linux`

---

## Deployment

**1. Copy the binary to the board:**

```bash
scp fan_control orangepi@<board-ip>:/home/orangepi/
```

**2. Install the binary:**

```bash
ssh orangepi@<board-ip>
sudo cp fan_control /usr/local/bin/fan_control
sudo chmod +x /usr/local/bin/fan_control
```

**3. Test run:**

```bash
sudo /usr/local/bin/fan_control
```

You should see temperature readings printed every 5 seconds. Ctrl+C to stop — the program will clean up GPIO on exit.

---

## Auto-Start on Boot (systemd)

**1. Create the service file:**

```bash
sudo nano /etc/systemd/system/fan_control.service
```

Paste the following:

```ini
[Unit]
Description=Fan Control Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/fan_control
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**2. Enable and start:**

```bash
sudo systemctl daemon-reload
sudo systemctl enable fan_control   # start on every boot
sudo systemctl start fan_control    # start right now
```

**3. Check status:**

```bash
sudo systemctl status fan_control

# Follow live logs
sudo journalctl -u fan_control -f
```

**4. Stop or disable:**

```bash
sudo systemctl stop fan_control     # stop now
sudo systemctl disable fan_control  # don't start on boot
```

> **Note:** The service runs as root by default, which is required to access `/sys/class/gpio`. `Restart=on-failure` will automatically restart the service if it crashes, but not if stopped manually with `systemctl stop`.