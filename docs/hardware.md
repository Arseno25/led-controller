# Hardware Guide

Dokumen ini menjelaskan wiring default firmware. Pin default berbeda per target
ESP32 family dan didefinisikan di:

`components/config_manager/include/board_profile.h`

LED dan audio pins dapat diubah dari Web UI dan tersimpan ke NVS. Pin GC9A01
saat ini adalah default compile-time per target.

## Supported Boards

Firmware sudah disiapkan untuk target ESP-IDF berikut:

| Target | Status |
| --- | --- |
| `esp32` | Original wiring, sudah diverifikasi build. |
| `esp32s2` | Target-aware default pins, sudah diverifikasi build. |
| `esp32s3` | Target-aware default pins, sudah diverifikasi build. |
| `esp32c3` | Target-aware default pins, sudah diverifikasi build. |
| `esp32c6` | Target-aware default pins, sudah diverifikasi build. |

Pastikan binary yang diflash sesuai target chip. Binary `esp32` tidak bisa
dipakai untuk `esp32s3` atau `esp32c3`.

## Power

Gunakan power supply eksternal untuk LED.

| LED type | Supply |
| --- | --- |
| WS2812B / SK6812 | 5V |
| WS2811 / Neon Pixel 12V | 12V |

Wajib:

- GND ESP32 dan GND power supply LED harus common ground.
- Pasang resistor 330-470 ohm di jalur data LED.
- Pasang kapasitor sekitar 1000 uF dekat input power LED.
- Jangan supply strip LED panjang dari pin 5V board ESP32.

## Default Pin Map

### ESP32

| Function | GPIO |
| --- | --- |
| LED data | 5 |
| INMP441 BCLK | 26 |
| INMP441 WS/LRCLK | 25 |
| INMP441 DOUT | 33 |
| GC9A01 SCLK | 18 |
| GC9A01 MOSI/SDA | 23 |
| GC9A01 CS | 15 |
| GC9A01 DC | 2 |
| GC9A01 RST | 4 |

### ESP32-S3 / ESP32-S2

| Function | GPIO |
| --- | --- |
| LED data | 5 |
| INMP441 BCLK | 6 |
| INMP441 WS/LRCLK | 7 |
| INMP441 DOUT | 15 |
| GC9A01 SCLK | 12 |
| GC9A01 MOSI/SDA | 11 |
| GC9A01 CS | 10 |
| GC9A01 DC | 9 |
| GC9A01 RST | 8 |

### ESP32-C3 / ESP32-C6

| Function | GPIO |
| --- | --- |
| LED data | 5 |
| INMP441 BCLK | 18 |
| INMP441 WS/LRCLK | 19 |
| INMP441 DOUT | 3 |
| GC9A01 SCLK | 6 |
| GC9A01 MOSI/SDA | 7 |
| GC9A01 CS | 10 |
| GC9A01 DC | 2 |
| GC9A01 RST | 4 |

Small C3/C6 boards often reserve pins for USB, boot, flash, or onboard devices.
Check the exact board schematic before wiring.

## LED Wiring

### WS2812B / SK6812

```text
ESP GPIO data  --[330R]-- DIN LED
ESP GND        ---------- GND power supply
LED 5V         ---------- 5V power supply
LED GND        ---------- GND power supply
```

### WS2811 / 12V Neon Pixel

```text
ESP GPIO data  --[330R]-- DIN LED
ESP GND        ---------- GND power supply
LED 12V        ---------- 12V power supply
LED GND        ---------- GND power supply
```

For 5V data into long 12V installations, use a proper level shifter/buffer if
the first pixel is unreliable.

## INMP441 Wiring

| INMP441 Pin | Connect To |
| --- | --- |
| VDD | 3.3V |
| GND | GND |
| SCK/BCLK | Target BCLK GPIO |
| WS/LRCLK | Target WS GPIO |
| SD/DOUT | Target DATA GPIO |
| L/R | GND or 3.3V, depending desired channel |

Use 3.3V only for INMP441.

## GC9A01 Wiring

Most GC9A01 modules label SPI data as `SDA`; for SPI this is MOSI, not I2C SDA.

| GC9A01 Pin | Connect To |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SCL/SCLK | Target TFT SCLK GPIO |
| SDA/MOSI | Target TFT MOSI GPIO |
| CS | Target TFT CS GPIO |
| DC | Target TFT DC GPIO |
| RST | Target TFT RST GPIO |
| BL/BLK | Not required by firmware; connect to 3.3V if the module exposes it |

This firmware does not use a dedicated BL/BLK GPIO. If the module has a BL/BLK
pad, connect it directly to 3.3V.

## GPIO Rules

The firmware validates LED pins using ESP-IDF GPIO capability macros instead of
a fixed ESP32-only whitelist:

- output pins must satisfy `GPIO_IS_VALID_OUTPUT_GPIO`
- input pins must satisfy `GPIO_IS_VALID_GPIO`

Avoid pins tied to flash, boot straps, USB, PSRAM, or onboard peripherals on
your exact development board.
