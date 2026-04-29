# HTTP API

Default SoftAP base URL:

`http://192.168.4.1`

All POST endpoints expect JSON and return JSON. Successful mutation responses
include at least:

```json
{
  "success": true,
  "message": "..."
}
```

Error responses include:

```json
{
  "success": false,
  "message": "..."
}
```

## Endpoint Summary

| Method | Path | Purpose |
| --- | --- | --- |
| GET | `/` | Embedded Web UI. |
| GET | `/api/config` | Full runtime config snapshot. |
| POST | `/api/config` | Update full or partial config and persist to NVS. |
| GET | `/api/led` | Alias for `/api/config`. |
| POST | `/api/led` | Alias for config update. |
| POST | `/api/color` | Update foreground RGBW color. |
| POST | `/api/brightness` | Update global LED brightness. |
| POST | `/api/power` | Toggle LED power. |
| POST | `/api/factory-reset` | Clear stored config and reboot/reset to defaults. |
| GET | `/api/animation` | Current normal animation settings. |
| POST | `/api/animation` | Update normal animation settings. |
| GET | `/api/animations` | Legacy list of normal animations. |
| GET | `/api/mode` | Current operating mode. |
| POST | `/api/mode` | Set operating mode. |
| GET | `/api/effects?category=...` | Effect registry by category. |
| POST | `/api/effect` | Set effect by category/name. |
| GET | `/api/reactive/effects` | Legacy reactive effect list. |
| POST | `/api/reactive/effect` | Legacy reactive effect setter. |
| GET | `/api/audio/config` | INMP441/audio processor config. |
| POST | `/api/audio/config` | Update audio config. |
| GET | `/api/audio/state` | Full realtime audio feature snapshot. |
| GET | `/api/audio/spectrum` | Spectrum-focused audio snapshot. |
| GET | `/api/matrix` | Matrix layout config. |
| POST | `/api/matrix` | Update matrix layout config. |
| GET | `/api/random` | Random/autoplay config. |
| POST | `/api/random` | Update random/autoplay config. |
| POST | `/api/random/next` | Switch to next random effect. |
| POST | `/api/random/reactive` | Legacy reactive random config update. |
| POST | `/api/random/normal` | Legacy normal random config update. |
| GET | `/api/palettes` | List built-in palettes. |
| POST | `/api/palette` | Set active palette. |
| GET | `/api/display` | GC9A01 display config. |
| POST | `/api/display` | Update display config. |
| POST | `/api/display/view` | Update display view mode only. |
| GET | `/api/system` | Runtime performance/health metrics. |

## Operating Modes

Use these values with `/api/mode`:

- `normal`
- `reactive`
- `matrix`
- `reactive_matrix`

Example:

```bash
curl -X POST http://192.168.4.1/api/mode \
  -H "Content-Type: application/json" \
  -d '{"operating_mode":"reactive"}'
```

## Effects

Categories:

- `normal`
- `reactive`
- `matrix`
- `reactive_matrix`

Effect metadata includes:

- `id`
- `name`
- `label`
- `category`
- `requires_matrix`
- `requires_audio`
- `supports_palette`
- `supports_color`
- `supports_random`

`supports_color` is used by the Web UI to hide color pickers when an effect is
palette/audio driven and does not use manual color.

Examples:

```bash
curl "http://192.168.4.1/api/effects?category=reactive"

curl -X POST http://192.168.4.1/api/effect \
  -H "Content-Type: application/json" \
  -d '{"category":"reactive","effect":"reactive_spectrum_bars"}'
```

## Config Snapshot

`GET /api/config` returns the main runtime config, including:

- `board`: target and target default pins
- LED type, pin, count, brightness, power
- normal animation/effect settings
- selected palette
- audio config
- matrix config
- random/autoplay config
- display config

The `board.target` field is compiled from the active ESP-IDF target.

## LED

```bash
curl -X POST http://192.168.4.1/api/brightness \
  -H "Content-Type: application/json" \
  -d '{"brightness":180}'

curl -X POST http://192.168.4.1/api/color \
  -H "Content-Type: application/json" \
  -d '{"r":0,"g":180,"b":255,"w":0}'

curl -X POST http://192.168.4.1/api/power \
  -H "Content-Type: application/json" \
  -d '{"power":true}'
```

## Audio

Audio config example:

```bash
curl -X POST http://192.168.4.1/api/audio/config \
  -H "Content-Type: application/json" \
  -d '{"i2s_bclk_pin":26,"i2s_ws_pin":25,"i2s_data_pin":33,"sample_rate":16000,"sensitivity":110,"gain":140,"auto_gain":true,"noise_gate":35,"smoothing":60,"beat_threshold":650,"fft_enabled":true,"spectrum_bands":16}'
```

Realtime audio state includes RMS, peak, smoothed level, volume, bass/mid/treble,
beat flag, onset flag, dominant frequency, spectral centroid, and spectrum
bands.

## Matrix

Matrix layout is optional and bounded to 32 x 32.

```bash
curl -X POST http://192.168.4.1/api/matrix \
  -H "Content-Type: application/json" \
  -d '{"enabled":true,"width":16,"height":16,"layout":"serpentine","origin":"top_left","reverse_x":false,"reverse_y":false,"rotate_90":false}'
```

## Display

Display config shape:

```json
{
  "enabled": true,
  "brightness": 180,
  "theme": "neon_dark",
  "viewMode": "auto",
  "showFps": true,
  "showWifi": true
}
```

Supported `viewMode` values:

- `auto`
- `status`
- `spectrum`
- `vu_meter`
- `waveform`

Examples:

```bash
curl http://192.168.4.1/api/display

curl -X POST http://192.168.4.1/api/display \
  -H "Content-Type: application/json" \
  -d '{"enabled":true,"brightness":180,"viewMode":"spectrum","showFps":true,"showWifi":true}'

curl -X POST http://192.168.4.1/api/display/view \
  -H "Content-Type: application/json" \
  -d '{"viewMode":"vu_meter"}'
```

## System Monitor

`GET /api/system` returns runtime metrics:

- uptime
- free heap and minimum free heap
- largest free block
- render frame count
- last/max/average render time
- render task stack watermark
- WiFi client count
- audio running flag
- config save state/count/error

Use this endpoint when checking realtime performance.
