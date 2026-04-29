# ESP32 LED Controller User Guide

Firmware LED controller untuk ESP32 family dengan Web UI lokal, mode basic,
reactive audio INMP441, matrix mode, dan display bulat GC9A01.

## Pilih Firmware Sesuai Board

Download ZIP firmware dari halaman GitHub Release sesuai chip board yang dipakai:

| Board / Chip | File ZIP |
| --- | --- |
| ESP32 DevKit / ESP32-WROOM | `led-controller-<version>-esp32.zip` |
| ESP32-S2 | `led-controller-<version>-esp32s2.zip` |
| ESP32-S3 | `led-controller-<version>-esp32s3.zip` |
| ESP32-C3 | `led-controller-<version>-esp32c3.zip` |
| ESP32-C6 | `led-controller-<version>-esp32c6.zip` |

Jangan flash firmware untuk target berbeda. Binary `esp32` tidak bisa dipakai
untuk `esp32s3`, `esp32c3`, atau target lain.

## Wiring Singkat

Gunakan power supply eksternal untuk LED dan sambungkan semua GND bersama.

### ESP32 Default

| Function | GPIO |
| --- | --- |
| LED data | 5 |
| INMP441 BCLK | 26 |
| INMP441 WS | 25 |
| INMP441 DOUT | 33 |
| GC9A01 SCLK | 18 |
| GC9A01 SDA/MOSI | 23 |
| GC9A01 CS | 15 |
| GC9A01 DC | 2 |
| GC9A01 RST | 4 |

### ESP32-S3 Default

| Function | GPIO |
| --- | --- |
| LED data | 5 |
| INMP441 BCLK | 6 |
| INMP441 WS | 7 |
| INMP441 DOUT | 15 |
| GC9A01 SCLK | 12 |
| GC9A01 SDA/MOSI | 11 |
| GC9A01 CS | 10 |
| GC9A01 DC | 9 |
| GC9A01 RST | 8 |

Untuk ESP32-S2/C3/C6, lihat [docs/hardware.md](docs/hardware.md).

GC9A01 `BL/BLK` tidak dipakai firmware. Jika modul punya pin itu, sambungkan ke
3.3V.

## Flash Firmware

Extract ZIP yang sesuai board, lalu flash dari dalam folder hasil extract.

Windows:

```bat
flash.bat COM5
```

Linux/macOS:

```bash
chmod +x flash.sh
./flash.sh /dev/ttyUSB0
```

Ganti `COM5` atau `/dev/ttyUSB0` sesuai port board.

## Connect ke Web UI

Setelah boot, controller membuat WiFi AP:

| Field | Value |
| --- | --- |
| SSID | `PixelController-Setup` |
| Password | `12345678` |
| Web UI | `http://192.168.4.1` |

Connect HP/laptop ke WiFi tersebut, lalu buka `http://192.168.4.1`.

## Setup Pertama

1. Buka Web UI.
2. Set LED type sesuai strip: WS2812B, WS2811, atau SK6812.
3. Set LED count sesuai jumlah LED.
4. Pastikan LED data pin benar.
5. Pilih mode:
   - `Normal` untuk efek basic.
   - `Reactive` untuk efek audio INMP441.
   - `Matrix` untuk layout panel.
   - `Reactive Matrix` untuk visualizer audio matrix.
6. Pilih effect.
7. Atur brightness, speed, palette, sensitivity, dan display.
8. Simpan konfigurasi.

Setting akan tersimpan, jadi setelah restart controller akan memakai setting
terakhir.

## Mode Utama

| Mode | Fungsi |
| --- | --- |
| Normal | Efek LED biasa seperti solid, rainbow, comet, fire, twinkle. |
| Reactive | Efek strip mengikuti audio dari INMP441. |
| Matrix | Efek untuk susunan LED panel 2D. |
| Reactive Matrix | Visualizer audio untuk matrix/panel. |

## Display GC9A01

Saat boot, display menampilkan animasi/status awal. Setelah boot, display
menampilkan visualizer dan status realtime seperti WiFi dan FPS.

Display bisa diatur dari Web UI:

- enable/disable
- brightness
- view mode: auto, status, spectrum, VU meter, waveform
- show FPS
- show WiFi status

## Tips Reactive Audio

- INMP441 harus pakai 3.3V.
- Jika reactive terlalu sensitif saat ruangan diam, naikkan noise gate.
- Jika kurang responsif, naikkan sensitivity atau gain.
- Jika gerakan terlalu kasar, naikkan smoothing.
- Reactive audio hanya aktif di mode `Reactive` dan `Reactive Matrix`.

## Troubleshooting

| Masalah | Cek |
| --- | --- |
| LED tidak menyala | Common ground, power supply, LED count, data pin. |
| Warna salah | LED type di Web UI sudah benar. |
| LED flicker | Pasang resistor data 330-470 ohm dan kapasitor 1000 uF. |
| Web UI tidak terbuka | Pastikan masih connect ke `PixelController-Setup`. |
| Reactive tidak bergerak | Cek wiring INMP441 dan mode sudah `Reactive`. |
| LCD blank | Cek pin SCLK, SDA/MOSI, CS, DC, RST, dan power 3.3V. |
| Salah firmware | Download ZIP sesuai target board lalu flash ulang. |

## Dokumentasi Lanjutan

- [Hardware detail](docs/hardware.md)
- [Cara pakai Web UI](docs/usage.md)
- [Build dan flash manual](docs/target-builds.md)
- [HTTP API](docs/api.md)
- [Arsitektur firmware](docs/architecture.md)

## License

This project is licensed under the [MIT License](LICENSE).
