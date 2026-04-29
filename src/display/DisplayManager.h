#pragma once

#include "audio_processor.h"
#include "config_manager.h"
#include "esp_err.h"
#include "system_monitor.h"

#ifdef __cplusplus
class DisplayManager {
public:
    void begin();
    void begin(const display_config_t &config);
    void update(const app_config_t &state, const audio_features_t &audio,
                const system_monitor_snapshot_t &system);
    void showBootScreen();
    void showBasicScreen(const app_config_t &state, const audio_features_t &audio,
                         const system_monitor_snapshot_t &system);
    void showReactiveScreen(const app_config_t &state, const audio_features_t &audio,
                            const system_monitor_snapshot_t &system);
    void setBacklight(uint8_t brightness);
    void applyConfig(const display_config_t &config);

private:
    esp_err_t initHardware();
    void drawStatusText(const app_config_t &state, const audio_features_t &audio,
                        const system_monitor_snapshot_t &system);
    void drawCircularVu(uint8_t level, bool beat);
    void drawSpectrumBars(const app_config_t &state, const audio_features_t &audio);
    void drawVuMeter(const audio_features_t &audio);
    void drawBeatPulse(bool beat, uint8_t level);
    void drawWaveform(uint8_t level);
    void drawBassMidTreble(const audio_features_t &audio);
    void drawTopStatus(const system_monitor_snapshot_t &system);
    void drawWifiIcon(int x, int y, bool connected);
    void drawText(int x, int y, const char *text, uint16_t color, uint8_t scale,
                  int maxWidth = 0);
    void drawTextCentered(int y, const char *text, uint16_t color, uint8_t scale);
    void drawTextCenteredFit(int y, const char *text, uint16_t color,
                             uint8_t preferredScale, uint8_t minScale, int maxWidth);
    void drawMetricBar(int x, int y, int w, const char *label, uint8_t value,
                       uint16_t color);
    void fillScreen(uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void drawRect(int x, int y, int w, int h, uint16_t color);
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
    void drawCircle(int cx, int cy, int r, uint16_t color, uint8_t thickness);
    void drawArc(int cx, int cy, int r, int startDeg, int endDeg, uint16_t color,
                 uint8_t thickness);
    void drawBar(int x, int y, int w, int h, uint8_t value, uint16_t color);
    uint32_t stateHash(const app_config_t &state, const audio_features_t &audio,
                       const system_monitor_snapshot_t &system) const;
    const char *modeLabel(operating_mode_t mode) const;
    const char *effectLabel(const app_config_t &state) const;

    unsigned long lastUpdate = 0;
    uint16_t frameInterval = 33;
    bool initialized = false;
    bool initFailed = false;
    display_config_t config = {};
    uint32_t lastStaticHash = 0;
    uint32_t lastDynamicMs = 0;
    uint32_t fpsWindowMs = 0;
    uint16_t fpsFrames = 0;
    uint16_t fps = 0;
    uint8_t smoothLevel = 0;
    uint8_t smoothBass = 0;
    uint8_t smoothMid = 0;
    uint8_t smoothTreble = 0;
    uint8_t smoothSpectrum[16] = {};
    uint8_t spectrumPeaks[16] = {};
    uint8_t visualFrame = 0;
    bool beatRingActive = false;
    uint8_t waveform[64] = {};
    uint8_t waveformPos = 0;
};
extern "C" {
#endif

esp_err_t display_manager_begin(const display_config_t *config);
void display_manager_apply_config(const display_config_t *config);
void display_manager_update(const app_config_t *state, const audio_features_t *audio,
                            const system_monitor_snapshot_t *system);
void display_manager_show_boot_screen(void);
void display_manager_set_backlight(uint8_t brightness);

#ifdef __cplusplus
}
#endif
