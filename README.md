# 🎮 ESP32 BLE Gamepad
Build your own Bluetooth gamepad with an ESP32 — pairs natively as HID, no drivers needed


<div align="center">

![Platform](https://img.shields.io/badge/platform-ESP32-blue?style=flat-square&logo=espressif)
![Protocol](https://img.shields.io/badge/protocol-BLE%20HID-4BADE8?style=flat-square&logo=bluetooth)
![Language](https://img.shields.io/badge/language-Arduino%20C%2B%2B-00599C?style=flat-square&logo=c%2B%2B)
![Library](https://img.shields.io/badge/library-ESP32--BLE--Gamepad-orange?style=flat-square)
![Status](https://img.shields.io/badge/status-working-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-lightgrey?style=flat-square)

**A DIY Bluetooth Low Energy gamepad built on an ESP32 — two analog joysticks, eight buttons via a 2×4 matrix, boot-time joystick calibration, and a cubic response curve for smooth axis feel.**

[Features](#-features) · [Hardware](#-hardware) · [Wiring](#-wiring) · [Software Setup](#-software-setup) · [How It Works](#-how-it-works) · [Troubleshooting](#-troubleshooting)

</div>

---

![Build photo](docs/images/build.jpg)

> 📸 _Photos and demo video coming soon — hardware is breadboarded and fully functional._

---

## ✨ Features

- **BLE HID** — pairs natively with Windows, Android, and Linux as a standard gamepad (no drivers)
- **2 analog joysticks** with boot-time center calibration — resting stick = exact zero, every time
- **Cubic response curve** — gentle near center, full deflection only with a real push (no axis snapping)
- **Voltage-corrected ADC mapping** — joystick modules run at 3.6 V from the ESP32; ADC range is adjusted accordingly so the full output range is still reachable
- **8 buttons** via a 2×4 matrix — only 6 GPIO pins for 8 inputs
- **2 joystick click buttons** (L3 / R3)
- **Hardware debouncing** in firmware — no external components needed
- **Connection indicator** — onboard LED blinks while searching, solid when paired

---

## 🛒 List of Materials

| Component | Qty | Notes |
|---|---|---|
| ESP32 Dev Board (38-pin) | 1 | Any standard ESP32-WROOM-32 module |
| Analog joystick module (KY-023 or equivalent) | 2 | VCC, GND, VRx, VRy, SW |
| Tactile push buttons | 8 | Any standard 6mm or 12mm momentary switch |
| Breadboard (full size) | 1 | Or custom PCB |
| Jumper wires | ~15 | Male-to-female |
| USB cable (Micro-B or Type-C) | 1 | For flashing and power |

> **No resistors needed** — the button matrix uses the ESP32's internal pull-up resistors on the column pins.

---

## 🔌 Wiring

### Joysticks

| Joystick Pin | ESP32 GPIO | Notes |
|---|---|---|
| **Left VRx** | GPIO 34 | Input-only ADC pin |
| **Left VRy** | GPIO 35 | Input-only ADC pin |
| **Left SW** | GPIO 25 | Joystick click (L3) |
| **Right VRx** | GPIO 32 | ADC pin |
| **Right VRy** | GPIO 33 | ADC pin |
| **Right SW** | GPIO 26 | Joystick click (R3) |
| **VCC (both)** | 3V3 | ⚠️ See voltage note below |
| **GND (both)** | GND | |

> ⚠️ **Voltage note:** These modules are rated for 5 V but work at 3.3 V with reduced ADC range. The firmware accounts for this — `ADC_MAX` is set to `3780` (not `4095`) to match the actual readable ceiling at 3.6 V supply. Do **not** connect VCC to the 5 V pin; the ESP32's ADC input max is 3.9 V and 5 V will damage it.

### Button Matrix (2 rows × 4 columns = 8 buttons)

```
              COL0    COL1    COL2    COL3
              GP13    GP12    GP14    GP27
ROW0  GP5  [  B1  ] [  B2  ] [  B3  ] [  B4  ]
ROW1  GP18 [  B5  ] [  B6  ] [  B7  ] [  B8  ]
```

| Role | GPIO Pins |
|---|---|
| **Row pins** (OUTPUT, driven LOW to scan) | GPIO 5, GPIO 18 |
| **Column pins** (INPUT_PULLUP, read state) | GPIO 13, GPIO 12, GPIO 14, GPIO 27 |

Each button connects **one leg to its row pin** and the **other leg to its column pin**. No resistors needed.

> ⚠️ **GPIO 12 note:** GPIO 12 is a strapping pin on ESP32. It must be LOW at boot (which it will be — column pins are pulled up, not driven). Avoid pulling it HIGH externally during power-on.

### BLE Button Map

| BLE Button # | Function |
|---|---|
| 1 | Left joystick click (L3) |
| 2 | Right joystick click (R3) |
| 3 – 10 | Matrix buttons B1 – B8 |

---

## 💻 Software Setup

### 1. Install Arduino IDE & ESP32 core

Add the ESP32 board manager URL in **File → Preferences**:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Then install **esp32 by Espressif Systems** via **Tools → Board → Board Manager**.

### 2. Install the BLE Gamepad library

In **Sketch → Include Library → Manage Libraries**, search for:
```
ESP32-BLE-Gamepad
```
Install **ESP32-BLE-Gamepad by lemmingDev**.

### 3. Flash the firmware

1. Clone this repo:
   ```bash
   git clone https://github.com/Jyot-Patel/esp32-ble-gamepad.git
   cd esp32-ble-gamepad
   ```
2. Open `esp32_ble_gamepad.ino` in Arduino IDE.
3. Select your board: **Tools → Board → ESP32 Dev Module**
4. Select the correct COM port.
5. Upload.

### 4. Pair

- On Windows: **Settings → Bluetooth → Add device → Bluetooth → "ESP32 Gamepad"**
- On Android: Standard Bluetooth settings — it appears as a gamepad/HID device.
- The onboard LED (GPIO 2) blinks while searching and goes solid when connected.

### 5. Steam & Applications

- Use Steam as a middleware to enable Xbox input from our custom Gamepad to play any of your favourite games.
- Steam also provides features like Calibration, deadzone avoidance, sensitivity to match user preference.
- Also can be used for various applications such as drone controller, Wireless Embedded Designs such as toy cars, robotic arm, etc working with Bluetooth.

> **Tip:** If Windows cached a previous (broken) HID descriptor, unpair the device, then in Device Manager → **View → Show hidden devices** → **Human Interface Devices**, delete any ghost entries for "ESP32 Gamepad", then re-pair.

---

## ⚙️ How It Works

### Boot calibration

On startup, the firmware samples each joystick axis 64 times over ~130 ms and averages the results to find the true resting center. This means physical center offset (joysticks rarely output exactly 2047 at rest) is fully compensated — no manual tuning needed. **Keep the sticks untouched during power-on.**

### Voltage-corrected ADC range

The joystick modules receive 3.6 V from the ESP32's 3V3 rail. With `ADC_11db` attenuation the ADC's full-scale is 3.9 V, giving an effective max reading of:

```
(3.6 / 3.9) × 4095 ≈ 3780
```

The old code used `4095` as the ceiling, meaning the firmware expected ADC readings the hardware could never produce — so the axis output hit maximum well before full physical deflection. `ADC_MAX = 3780` fixes this.

### Cubic response curve

After dead zone filtering, deflection is normalized to `[0.0, 1.0]` and passed through:

```
output = t³
```

| Stick deflection | Linear output (old) | Cubic output (new) |
|---|---|---|
| 10% past deadzone | 10% | 0.1% |
| 50% | 50% | 12.5% |
| 80% | 80% | 51.2% |
| 100% | 100% | 100% |

Small nudges stay small. You need a real push to reach max output. To soften the curve, swap to `t²` (quadratic) or blend: `0.3*t + 0.7*t*t*t`.

### Button matrix scanning

The firmware drives one row LOW at a time and reads all column pins. A column reads LOW only when the button at that intersection is pressed (completing the circuit through the LOW row). All other rows are HIGH, so only one row's buttons are active at a time. This cycle repeats every `POLL_MS` (10 ms).

---

## 🛠 Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Device doesn't appear in Bluetooth scan | Not advertising yet | Wait ~5 s after power-on; LED should blink |
| Pairs but axes don't move | Wrong ADC pins / joystick not powered | Check VCC/GND on joystick; confirm GPIO 34/35/32/33 |
| Press Windows+R and type joy.cpl to view custom gaming HID controller. Calibrate it with Windows settings | 
| Axis snaps to max immediately | Old cached HID descriptor on Windows | Unpair → delete ghost in Device Manager → re-pair |
| Buttons not registering | Matrix wiring | Confirm row pins are OUTPUT, col pins are INPUT_PULLUP |
| Left stick drifts at rest | Joystick not centered during boot calibration | Power cycle with sticks untouched for first ~2 s |
| Axis output feels sluggish | Cubic curve too aggressive | Change `t*t*t` to `t*t` in `toAxis()` |


---

## 📜 License

MIT — do whatever you want, attribution appreciated.

---

<div align="center">

Built by [Jyot-Patel](https://github.com/Jyot-Patel) · If this helped you, leave a ⭐

</div>
