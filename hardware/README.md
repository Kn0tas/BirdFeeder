# BirdFeeder — PCB Schematic Build Guide (EasyEDA)

This guide contains every connection, component value, and pin mapping needed to draw the BirdFeeder schematic in EasyEDA. Follow each sheet section in order.

> **Source of truth** for GPIO assignments: [`main/board_config.h`](../main/board_config.h)

---

## Sheet 1 — Power

### USB-C Receptacle (J1)

Provides both programming (data) and charging (power).

| USB-C Pin | Connects To                                   |
| --------- | --------------------------------------------- |
| VBUS      | bq24074 `IN` pin (via ferrite bead or direct) |
| D+        | ESP32-S3 **GPIO 20**                          |
| D−        | ESP32-S3 **GPIO 19**                          |
| CC1       | 5.1kΩ resistor to GND                         |
| CC2       | 5.1kΩ resistor to GND                         |
| GND       | Ground plane                                  |

> The two 5.1kΩ resistors on CC1/CC2 tell USB-C hosts that this is a sink device (draws power).

### Solar Input Connector (J2) — JST-PH 2-Pin

| Pad       | Connects To                                                               |
| --------- | ------------------------------------------------------------------------- |
| + (Red)   | bq24074 `IN` pin, through **Schottky diode D1** (anode=solar, cathode=IN) |
| − (Black) | GND                                                                       |

> The Schottky diode prevents USB VBUS from backfeeding into the solar panel when USB is plugged in. Use an SS14 or equivalent (1A, 40V).

### LiPo Battery Connector (J3) — JST-PH 2-Pin

| Pad       | Connects To       |
| --------- | ----------------- |
| + (Red)   | bq24074 `BAT` pin |
| − (Black) | GND               |

### Charger IC — TI bq24074 (U1)

| bq24074 Pin | Connects To                             | Notes                                                                      |
| ----------- | --------------------------------------- | -------------------------------------------------------------------------- |
| IN          | USB VBUS + Solar (via D1)               | Input supply, 4.35–10V                                                     |
| BAT         | LiPo battery + terminal                 | Direct to battery                                                          |
| OUT         | 3.3V LDO input (U2 VIN), Servo VCC (J5) | Load output, max 4.4V                                                      |
| VSS (GND)   | Ground plane                            | Multiple pads — connect all                                                |
| ISET        | R_ISET to GND                           | **590Ω → ~1A** charge current (see table below)                            |
| CE          | 3.3V (or leave floating with pull-up)   | Chip enable, active high                                                   |
| TS          | 100kΩ resistor to GND                   | Thermistor input — if no thermistor, tie to GND via 100kΩ to simulate 25°C |
| PG          | LED1 anode (via 1kΩ to 3.3V)            | Power Good indicator (open-drain, active low)                              |
| CHG         | LED2 anode (via 1kΩ to 3.3V)            | Charge indicator (open-drain, active low)                                  |
| TMR         | 10kΩ to GND                             | Safety timer (or float to disable)                                         |
| EN1, EN2    | Both to 3.3V                            | Enable USB-level current (see datasheet Table 1)                           |

**ISET Charge Current Reference:**

| R_ISET | Charge Current |
| ------ | -------------- |
| 1.18kΩ | 500 mA         |
| 590Ω   | 1000 mA        |
| 390Ω   | 1500 mA        |

**Decoupling:**

- 10µF ceramic on `IN`
- 10µF ceramic on `BAT`
- 10µF ceramic on `OUT`

### 3.3V LDO Regulator — AMS1117-3.3 or AP2112K-3.3 (U2)

| Pin  | Connects To                      |
| ---- | -------------------------------- |
| VIN  | bq24074 `OUT`                    |
| VOUT | 3.3V rail (MCU, sensors, camera) |
| GND  | Ground plane                     |

**Decoupling:**

- 10µF ceramic on VIN
- 10µF ceramic + 100nF ceramic on VOUT

---

## Sheet 2 — MCU

### ESP32-S3-WROOM-1 N8R8 (U3)

**Power:**

- VDD3P3 → 3.3V rail
- GND → Ground plane (connect ALL GND pads)
- Decoupling: 10µF + 100nF on VDD3P3, placed as close to the module as possible

**Programming (Native USB):**

- GPIO 19 → USB-C D−
- GPIO 20 → USB-C D+

**Reset Circuit (SW1):**

- EN pin → 10kΩ pull-up to 3.3V
- EN pin → 100nF cap to GND (debounce)
- EN pin → Tactile button to GND

**Boot/Flash Circuit (SW2):**

- GPIO 0 → 10kΩ pull-up to 3.3V
- GPIO 0 → Tactile button to GND

**GPIO Allocation:**

| GPIO | Net Name  | Direction | Destination                 |
| ---- | --------- | --------- | --------------------------- |
| 0    | BOOT      | Input     | Boot button (active low)    |
| 1    | CAM_RESET | Output    | Camera FPC pin 17           |
| 2    | SERVO_PWM | Output    | Servo header J5 signal      |
| 4    | CAM_PWDN  | Output    | Camera FPC pin 18           |
| 5    | FUEL_ALRT | Input     | MAX17048 ALRT (active low)  |
| 6    | CAM_VSYNC | Input     | Camera FPC pin 5            |
| 7    | CAM_HREF  | Input     | Camera FPC pin 6            |
| 8    | I2C_SDA   | Bidir     | I2C bus (MAX17048 + FRAM)   |
| 9    | I2C_SCL   | Output    | I2C bus (MAX17048 + FRAM)   |
| 10   | CAM_XCLK  | Output    | Camera FPC pin 8            |
| 11   | CAM_D2    | Input     | Camera FPC pin 16           |
| 12   | CAM_PCLK  | Input     | Camera FPC pin 7            |
| 13   | CAM_D3    | Input     | Camera FPC pin 15           |
| 14   | CAM_D6    | Input     | Camera FPC pin 12           |
| 15   | CAM_D7    | Input     | Camera FPC pin 11           |
| 16   | PIR_OUT   | Input     | PIR sensor AM312            |
| 17   | CAM_D5    | Input     | Camera FPC pin 13           |
| 18   | CAM_D4    | Input     | Camera FPC pin 14           |
| 19   | USB_DN    | Bidir     | USB-C D−                    |
| 20   | USB_DP    | Bidir     | USB-C D+                    |
| 38   | CAM_SIOD  | Bidir     | Camera FPC pin 4 (SCCB SDA) |
| 39   | CAM_SIOC  | Output    | Camera FPC pin 3 (SCCB SCL) |
| 47   | CAM_D9    | Input     | Camera FPC pin 9            |
| 48   | CAM_D8    | Input     | Camera FPC pin 10           |

---

## Sheet 3 — Camera

### 24-Pin 0.5mm FPC Connector (J4)

Use a **bottom-contact, 24-pin, 0.5mm pitch** FPC connector. All signals are routed as copper traces (no pin headers).

> ⚠️ **Verify the pin numbering** against your specific OV2640 module's datasheet. The mapping below follows the most common layout.

| FPC Pin | Signal     | ESP32 GPIO | Notes              |
| ------- | ---------- | ---------- | ------------------ |
| 1       | VCC (3.3V) | —          | 3.3V rail          |
| 2       | GND        | —          | Ground             |
| 3       | SIOC (SCL) | 39         | SCCB clock         |
| 4       | SIOD (SDA) | 38         | SCCB data          |
| 5       | VSYNC      | 6          | Frame sync         |
| 6       | HREF       | 7          | Line valid         |
| 7       | PCLK       | 12         | Pixel clock        |
| 8       | XCLK       | 10         | Master clock out   |
| 9       | D9         | 47         | Data bit 7         |
| 10      | D8         | 48         | Data bit 6         |
| 11      | D7         | 15         | Data bit 5         |
| 12      | D6         | 14         | Data bit 4         |
| 13      | D5         | 17         | Data bit 3         |
| 14      | D4         | 18         | Data bit 2         |
| 15      | D3         | 13         | Data bit 1         |
| 16      | D2         | 11         | Data bit 0         |
| 17      | RESET      | 1          | Active low reset   |
| 18      | PWDN       | 4          | Power down control |
| 19–24   | GND / NC   | —          | Ground or unused   |

**Layout notes:**

- Keep data lines (D2–D9) as short and equal-length as possible
- Route XCLK away from data lines to minimize crosstalk
- Place a 100nF bypass cap near pin 1 (VCC)

---

## Sheet 4 — Sensors & Actuators

### I2C Bus

| Component | Value | Connects                     |
| --------- | ----- | ---------------------------- |
| R1        | 4.7kΩ | SDA (GPIO 8) pull-up to 3.3V |
| R2        | 4.7kΩ | SCL (GPIO 9) pull-up to 3.3V |

Bus speed: 400 kHz (Fast Mode).

### Fuel Gauge — MAX17048 (U4)

| MAX17048 Pin | Connects To                                                                                       |
| ------------ | ------------------------------------------------------------------------------------------------- |
| CELL+        | LiPo battery positive (or bq24074 `BAT` pad)                                                      |
| CELL−        | GND                                                                                               |
| VDD          | 3.3V rail                                                                                         |
| SDA          | GPIO 8 (shared I2C bus)                                                                           |
| SCL          | GPIO 9 (shared I2C bus)                                                                           |
| ALRT         | GPIO 5 (active low, open-drain — needs pull-up; can share R1 on SDA or add separate 10kΩ to 3.3V) |
| QST          | NC (leave unconnected)                                                                            |

I2C address: **0x36** (fixed).

Decoupling: 100nF on VDD.

### FRAM — MB85RC256V (U5)

| MB85RC256V Pin | Connects To                     |
| -------------- | ------------------------------- |
| VCC            | 3.3V rail                       |
| GND            | GND                             |
| SDA            | GPIO 8 (shared I2C bus)         |
| SCL            | GPIO 9 (shared I2C bus)         |
| WP             | GND (write protection disabled) |
| A0             | GND                             |
| A1             | GND                             |
| A2             | GND                             |

I2C address: **0x50** (A0=A1=A2=GND).

Decoupling: 100nF on VCC.

### PIR Motion Sensor — AM312 (3-Pin Header, J6)

| Header Pin | Connects To |
| ---------- | ----------- |
| VCC        | 3.3V rail   |
| OUT        | GPIO 16     |
| GND        | GND         |

> The AM312 has a built-in pull-up. No external resistor needed.

### Servo — SG90 (3-Pin Header, J5)

| Header Pin             | Connects To               |
| ---------------------- | ------------------------- |
| Signal (Orange/Yellow) | GPIO 2                    |
| VCC (Red)              | bq24074 `OUT` (~3.7–4.2V) |
| GND (Brown)            | GND                       |

> The SG90 operates at 4.8–6V but works reliably at battery voltage (3.7–4.2V). Do NOT power from the 3.3V rail — it draws too much current.

---

## Status LEDs (Optional)

| LED               | Connected To               | Resistor | Meaning                   |
| ----------------- | -------------------------- | -------- | ------------------------- |
| LED1 (Green)      | bq24074 `PG` → 1kΩ → 3.3V  | 1kΩ      | ON = Power Good           |
| LED2 (Red/Orange) | bq24074 `CHG` → 1kΩ → 3.3V | 1kΩ      | ON = Charging, OFF = Done |

Both PG and CHG are open-drain active low, so wire as: 3.3V → 1kΩ → LED anode → LED cathode → PG/CHG pin.

---

## Design Checklist

Before running EasyEDA's ERC, verify:

- [ ] All ESP32-S3 GND pads connected to ground plane
- [ ] Decoupling caps placed close to U1, U2, U3, U4, U5
- [ ] I2C pull-ups (R1, R2) present on SDA/SCL
- [ ] USB CC1/CC2 have 5.1kΩ to GND
- [ ] Schottky diode D1 on solar input (anode=solar, cathode=bq24074 IN)
- [ ] bq24074 ISET resistor matches desired charge current
- [ ] bq24074 TS pin has 100kΩ to GND (if no thermistor)
- [ ] EN button has RC debounce (10kΩ + 100nF)
- [ ] BOOT button has 10kΩ pull-up
- [ ] MAX17048 CELL+ goes to battery positive, NOT to charger OUT
- [ ] Servo VCC from bq24074 OUT, NOT from 3.3V rail
- [ ] FPC pinout verified against your specific OV2640 module datasheet
