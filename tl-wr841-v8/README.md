# OpenWrt GPIO Playground for TL-WR841N/ND v8

GPIO control experiments on an old router: the built-in LED, two types of stepper motors, and an LCD 1602.

| Parameter | Value |
| :--- | :--- |
| Router | TP-Link TL-WR841N/ND v8 |
| SoC | Atheros AR9341, MIPS 24Kc |
| RAM / Flash | 32 MB / 4 MB |
| Tested firmware | OpenWrt 19.07.10, `ar71xx/tiny` |
| GPIO logic level | 3.3 V |

## Programs

| Source file | Description |
| :--- | :--- |
| `src/blink.c` | Blinks DS2, the first LED after POWER. |
| `src/uln2003.c` | Controls a 28BYJ-48 stepper motor through a ULN2003. |
| `src/a4988_ctrl.c` | Controls a NEMA17 stepper motor through an A4988. |
| `src/1602_lcd.c` | Displays text typed in the SSH terminal on an LCD 1602. |

Source files are stored in `src/`; compiled MIPS binaries go in `dist/`.

## Firmware

This project used an archive from the 4PDA thread:

- [TP-Link TL-WR841 thread](https://4pda.to/forum/index.php?showtopic=473913);
- [post containing the archive used for this project](https://4pda.to/forum/index.php?showtopic=473913&st=17360#entry114863108).

The archive contains `factory.bin` and `sysupgrade.bin`:

- `factory.bin` is used for the first installation from the original TP-Link firmware;
- `sysupgrade.bin` updates an existing OpenWrt installation.


For the first installation, connect the computer to a LAN port, open the original web interface (`192.168.0.1` or `192.168.1.1`), and upload the appropriate `factory` image through **System Tools → Firmware Upgrade**. Do not disconnect power until flashing and rebooting are complete. After the first login, immediately set the `root` password to 1.

## LEDs and GPIO

LED order from left to right on the board:

```text
DS1, DS2, DS7, DS3, DS4, DS5, DS6, DS8
```

| Position from left | Silkscreen | Indicator | GPIO |
| :---: | :---: | :--- | :---: |
| 1 | DS1 | POWER | 14 |
| 2 | DS2 | WLAN | 13 |
| 3 | DS7 | WAN | 18 |
| 4 | DS3 | LAN1 | 19 |
| 5 | DS4 | LAN2 | 20 |
| 6 | DS5 | LAN3 | 21 |
| 7 | DS6 | LAN4 | 12 |
| 8 | DS8 | QSS | 15 |

Do not connect the GPIO pins to 5 V signals; this can damage the SoC.

## Hardware modification

DS1 remains on the board. Desolder DS2, DS7, DS3, DS4, DS5, DS6, and DS8 to expose external signals.

Solder one pin from a 2.54 mm single-row male pin header to the cathode pad of each removed LED. Dupont wires can then connect these pins to a circuit assembled on a solderless breadboard.

All modules, external power supplies, and the router must share a common ground.

## Preparing the GPIO pins

After every reboot, first release the GPIO pins from the LED driver:

```sh
echo leds-gpio > /sys/bus/platform/drivers/leds-gpio/unbind
```

Then export and initialize all GPIO pins with one command:

```sh
for pin in 13 18 19 20 21 12 15; do echo "$pin" > /sys/class/gpio/export 2>/dev/null; echo out > "/sys/class/gpio/gpio$pin/direction"; echo 0 > "/sys/class/gpio/gpio$pin/value"; done
```

`blink` uses the standard LED driver and must be run **before** `unbind`. After `unbind`, it will not be available again until the router is rebooted or the driver is bound again.

### Testing the soldered header pins

After soldering, first measure the voltage on every header pin. Set the multimeter to DC voltage, connect the black probe to GND, and touch the red probe to the pin under test. After the export command above, every GPIO should be at logic low, with a voltage close to 0 V.

Then run the sequential test:

```sh
for x in 13:DS2 18:DS7 19:DS3 20:DS4 21:DS5 12:DS6 15:DS8; do p=${x%%:*}; n=${x##*:}; for q in 13 18 19 20 21 12 15; do echo 0 > /sys/class/gpio/gpio$q/value; done; echo 1 > /sys/class/gpio/gpio$p/value; echo "ON: $n (gpio$p). Press Enter to continue"; read r; done; for q in 13 18 19 20 21 12 15; do echo 0 > /sys/class/gpio/gpio$q/value; done; echo "Done"
```

The command leaves only one GPIO high at a time and waits for Enter. During each pause, measure the voltage on the indicated pin; all other pins should remain close to 0 V. After the final check, the command sets every GPIO low again.

On the router used for this project, logic high measured approximately 2.5 V instead of 3.3 V. Check the actual voltage before connecting a module and make sure its inputs reliably recognize this level as logic high.

## DS2: blinking an LED

`blink` controls DS2 through `/sys/class/leds/tp-link:green:wlan`. On exit, including `Ctrl+C`, the program restores the original brightness and LED trigger.

```sh
/tmp/blink
/tmp/blink 20 100
```

The first argument is the number of blinks. The second is the on and off duration in milliseconds.

## 28BYJ-48 + ULN2003

| ULN2003 | Connect to the router |
| :---: | :--- |
| IN1 | DS4 (GPIO20) |
| IN2 | DS5 (GPIO21) |
| IN3 | DS6 (GPIO12) |
| IN4 | DS8 (GPIO15) |
| GND | GND |
| VCC | external 5 V supply |

Do not power the motor from the router. Connect the ground of the external 5 V supply to the router GND.

```sh
/tmp/uln2003 2000
```

The argument is the delay between half-steps in microseconds. Left/Right starts rotation; Up/Down stops it. `Ctrl+C` exits and turns off all four coils.

## NEMA17 + A4988

| A4988 | Connect to the router or power supply |
| :---: | :--- |
| STEP | DS4 (GPIO20) |
| DIR | DS5 (GPIO21) |
| ENABLE | DS6 (GPIO12) |
| 1A, 1B | first NEMA17 winding |
| 2A, 2B | second NEMA17 winding |
| GND | router GND and motor power supply GND |
| VDD | 3.3 V |
| VMOT | separate motor power supply, usually 12 V |

Connect `RESET` and `SLEEP` together and pull them up to `VDD`. For full-step mode, connect `MS1`, `MS2`, and `MS3` to GND. Install an electrolytic capacitor of at least 100 µF close to `VMOT/GND`. Never connect or disconnect the motor while it is powered.

### NEMA17 windings

Disconnect the motor and set the multimeter to continuity or resistance mode. Two wires from the same winding show low resistance and usually trigger the continuity beeper; wires from different windings show an open circuit. Find both pairs this way. Connect one pair to `1A/1B` and the other to `2A/2B`. Swapping the two wires of either pair reverses the rotation direction. Never rearrange motor wires while the A4988 is powered.

### Current limit adjustment

Before connecting the motor, set `VREF` to approximately **0.65 V**:

1. Disconnect the motor from the A4988 and supply the module with `VDD=3.3 V` and GND.
2. Set the multimeter to DC voltage mode.
3. Connect the black probe to GND and the red probe to the metal wiper of the adjustment potentiometer (`VREF`).
4. Slowly turn the adjustment potentiometer until the multimeter reads 0.65 V.

```sh
/tmp/a4988_ctrl 1000
/tmp/a4988_ctrl 1000 --reverse
```

The argument is the pause between STEP pulses in microseconds. Left/Right starts rotation; Up/Down stops it. `Ctrl+C` exits, sets STEP=0, and disables the driver outputs with ENABLE=1.

## LCD 1602

| LCD 1602 | Connect to the router or power supply |
| :---: | :--- |
| 1 VSS | GND |
| 2 VDD | +5 V |
| 3 V0 | wiper of a 10 kΩ potentiometer connected between +5 V and GND |
| 4 RS | DS7 (GPIO18) |
| 5 RW | GND |
| 6 E | DS3 (GPIO19) |
| 11 D4 | DS4 (GPIO20) |
| 12 D5 | DS5 (GPIO21) |
| 13 D6 | DS6 (GPIO12) |
| 14 D7 | DS8 (GPIO15) |
| 15 A | +5 V through 220 Ω if the module has no built-in resistor |
| 16 K | GND |

Different LCD 1602 modules have different logic-high thresholds. If a display powered from 5 V does not reliably recognize 3.3 V signals, add a 3.3 V to 5 V level shifter between the GPIO pins and LCD inputs. `RW` is permanently connected to GND, so the display cannot drive 5 V back into the router.

The HD44780 stores eight custom characters. The display can show no more than eight unique Cyrillic letters without a visually equivalent Latin character at the same time; additional characters are displayed as `?`.

```sh
/tmp/1602_lcd
```

Enter moves the cursor to the next line or scrolls the display. `Ctrl+C` exits, clears the display, sets E=0, and restores the terminal mode.

## Compilation

Download and unpack the OpenWrt 19.07.10 SDK for `ar71xx/tiny`:

```sh
wget https://downloads.openwrt.org/releases/19.07.10/targets/ar71xx/tiny/openwrt-sdk-19.07.10-ar71xx-tiny_gcc-7.5.0_musl.Linux-x86_64.tar.xz
tar -xf openwrt-sdk-19.07.10-ar71xx-tiny_gcc-7.5.0_musl.Linux-x86_64.tar.xz
cd openwrt-sdk-19.07.10-ar71xx-tiny_gcc-7.5.0_musl.Linux-x86_64
export STAGING_DIR="$PWD/staging_dir"
export PATH="$PWD/staging_dir/toolchain-mips_24kc_gcc-7.5.0_musl/bin:$PATH"
```

Return to the `tl-wr841-v8` directory and build every program with one command:

```sh
for name in blink uln2003 a4988_ctrl 1602_lcd; do mips-openwrt-linux-musl-gcc -static -O2 -std=gnu11 -Wall -Wextra -o "dist/$name" "src/$name.c"; done
```

## Uploading to the router

```sh
scp dist/blink dist/uln2003 dist/a4988_ctrl dist/1602_lcd root@192.168.1.1:/tmp/
ssh root@192.168.1.1
chmod +x /tmp/blink /tmp/uln2003 /tmp/a4988_ctrl /tmp/1602_lcd
```

`/tmp` is cleared whenever the router reboots.
