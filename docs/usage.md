# Usage Guide

## First Boot

After flashing, the controller starts a WiFi access point:

| Field | Value |
| --- | --- |
| SSID | `PixelController-Setup` |
| Password | `12345678` |
| Gateway | `192.168.4.1` |
| Web UI | `http://192.168.4.1` |

Connect a phone or laptop to the AP, then open `http://192.168.4.1`.

## Basic Workflow

1. Open the Web UI.
2. Confirm the detected board target shown in the device/status area.
3. Configure LED type, LED count, and LED data pin.
4. Pick mode: `normal`, `reactive`, `matrix`, or `reactive_matrix`.
5. Select an effect.
6. Adjust brightness, speed, palette, and audio sensitivity as needed.
7. Save/apply changes from the Web UI.

Most runtime changes apply immediately and are persisted to NVS.

## Web UI Areas

The UI is organized around practical controller workflows:

| Area | Purpose |
| --- | --- |
| Power/brightness/color | Fast LED runtime control. |
| Mode/effect | Select normal, reactive, matrix, or reactive matrix behavior. |
| Effect options | Speed, size, density, palette, direction, and similar parameters. |
| Audio | INMP441 pins and audio processing sensitivity/gain/gate settings. |
| Matrix | Matrix dimensions and physical layout mapping. |
| Display | GC9A01 enable, brightness, view mode, FPS/WiFi overlay. |
| Random/autoplay | Automatic effect switching. |
| System | Runtime state such as FPS, WiFi clients, heap, and audio status. |

The color picker is hidden for effects that do not support manual color, based
on effect metadata from `/api/effects`.

## Modes

### Normal

Uses `animation_layer` for non-audio strip effects.

Use for:

- solid color
- rainbow
- comet
- fire
- twinkle/sparkle
- other basic strip effects

### Reactive

Uses INMP441 audio and `reactive_renderer`.

Use for:

- VU bar
- pulse
- beat flash
- spark
- comet/chase
- spectrum bars
- bass hit

### Matrix

Uses `matrix_engine` and matrix layout config.

Use when the LEDs are arranged as a 2D panel.

### Reactive Matrix

Uses INMP441 audio and `reactive_matrix_renderer`.

Use for matrix audio visualizers such as spectrum bars, center VU, audio ripple,
fire EQ, bass tunnel, and similar effects.

## GC9A01 Display

At boot, the display shows a short boot animation/status screen. After boot,
the display focuses on minimal visual output:

- realtime WiFi status
- realtime FPS
- audio level/spectrum visualizer
- mode-aware visual behavior

Display settings are available through Web UI and `/api/display`.

View modes:

- `auto`: firmware chooses view based on active mode
- `status`: compact status view
- `spectrum`: spectrum visualization
- `vu_meter`: VU style visualization
- `waveform`: waveform visualization

## Audio Setup

For reactive modes:

1. Wire INMP441 according to `docs/hardware.md`.
2. Confirm I2S pins in the Web UI.
3. Start with default sensitivity/gain.
4. If the LED reacts while silent, raise noise gate.
5. If it barely reacts, increase sensitivity or gain.
6. If reaction is too jumpy, increase smoothing.

Reactive audio only runs while mode is `reactive` or `reactive_matrix`.

## Matrix Setup

Set:

- width
- height
- layout: `serpentine` or `progressive`
- origin
- reverse X/Y
- rotate 90

If the image moves in the wrong direction, first adjust origin/reverse flags
before changing the physical wiring.

## Persistence

Config is saved to NVS. After reset or power cycle, the firmware restores the
last saved settings.

If config becomes invalid or corrupted, firmware falls back to defaults instead
of crashing.

## Factory Reset

Use the Web UI Factory Reset action, or call:

```bash
curl -X POST http://192.168.4.1/api/factory-reset
```

## Troubleshooting

| Symptom | Check |
| --- | --- |
| LED off | Common ground, correct data pin, enough power, LED count not zero. |
| Wrong colors | Correct LED type: WS2812B/WS2811/SK6812 RGB/RGBW. |
| Flicker | Add resistor, add capacitor, shorten data wire, use level shifter if needed. |
| No AP | Check serial logs and power stability. |
| Web UI fetch/network errors | Confirm phone/laptop is connected to `PixelController-Setup` and open `http://192.168.4.1`. |
| Reactive not moving | Check INMP441 3.3V, BCLK/WS/DOUT pins, mode set to `reactive`. |
| Display blank | Confirm GC9A01 SCLK/MOSI/CS/DC/RST pins for the selected target and power at 3.3V. |
| Display mirrored/rotated | Adjust display orientation in firmware if physical module orientation differs. |
| Save failed | Check JSON payload and GPIO validity for the current ESP-IDF target. |
