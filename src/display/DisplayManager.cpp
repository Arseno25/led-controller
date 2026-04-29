#include "DisplayManager.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DisplayTheme.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display";
static DisplayManager s_display;
static spi_device_handle_t s_spi = NULL;
static bool s_bus_initialized = false;
static const display_theme_colors_t &C = DISPLAY_THEME_NEON_DARK_COLORS;
#if defined(CONFIG_IDF_TARGET_ESP32)
static constexpr spi_host_device_t DISPLAY_SPI_HOST = SPI3_HOST; /* ESP32 VSPI: SCL=18, SDA=23 */
#else
static constexpr spi_host_device_t DISPLAY_SPI_HOST = SPI2_HOST;
#endif

static uint16_t to_be(uint16_t c)
{
    return (uint16_t)((c >> 8) | (c << 8));
}

static uint32_t now_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static uint8_t smooth_u8(uint8_t current, uint8_t target)
{
    if (target > current) {
        return (uint8_t)(current + ((target - current + 1) / 2));
    }
    return (uint8_t)(current - ((current - target + 3) / 4));
}

static int text_width_px(const char *text, uint8_t scale)
{
    if (!text || scale == 0) return 0;
    int len = (int)strlen(text);
    return len > 0 ? (len * 6 * scale - scale) : 0;
}

static esp_err_t spi_write(bool data_mode, const void *data, size_t len)
{
    if (!s_spi || !data || len == 0) return ESP_ERR_INVALID_ARG;
    gpio_set_level((gpio_num_t)DISPLAY_TFT_DC_PIN, data_mode ? 1 : 0);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    return spi_device_polling_transmit(s_spi, &t);
}

static esp_err_t write_cmd(uint8_t cmd)
{
    return spi_write(false, &cmd, 1);
}

static esp_err_t write_data(const void *data, size_t len)
{
    return spi_write(true, data, len);
}

static esp_err_t write_cmd_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    esp_err_t err = write_cmd(cmd);
    if (err == ESP_OK && len > 0) err = write_data(data, len);
    return err;
}

static void set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];
    data[0] = (uint8_t)(x0 >> 8); data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8); data[3] = (uint8_t)x1;
    write_cmd_data(0x2A, data, sizeof(data));
    data[0] = (uint8_t)(y0 >> 8); data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8); data[3] = (uint8_t)y1;
    write_cmd_data(0x2B, data, sizeof(data));
    write_cmd(0x2C);
}

#define GLYPH5(a,b,c,d,e,f,g) \
    ((((uint64_t)(a) & 0x1F) << 30) | (((uint64_t)(b) & 0x1F) << 25) | \
     (((uint64_t)(c) & 0x1F) << 20) | (((uint64_t)(d) & 0x1F) << 15) | \
     (((uint64_t)(e) & 0x1F) << 10) | (((uint64_t)(f) & 0x1F) << 5)  | \
     (((uint64_t)(g) & 0x1F)))

static uint64_t glyph5x7(char raw)
{
    char c = (char)toupper((unsigned char)raw);
    switch (c) {
        case 'A': return GLYPH5(0x0E,0x11,0x11,0x1F,0x11,0x11,0x11);
        case 'B': return GLYPH5(0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E);
        case 'C': return GLYPH5(0x0E,0x11,0x10,0x10,0x10,0x11,0x0E);
        case 'D': return GLYPH5(0x1E,0x11,0x11,0x11,0x11,0x11,0x1E);
        case 'E': return GLYPH5(0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F);
        case 'F': return GLYPH5(0x1F,0x10,0x10,0x1E,0x10,0x10,0x10);
        case 'G': return GLYPH5(0x0E,0x11,0x10,0x17,0x11,0x11,0x0F);
        case 'H': return GLYPH5(0x11,0x11,0x11,0x1F,0x11,0x11,0x11);
        case 'I': return GLYPH5(0x1F,0x04,0x04,0x04,0x04,0x04,0x1F);
        case 'J': return GLYPH5(0x07,0x02,0x02,0x02,0x12,0x12,0x0C);
        case 'K': return GLYPH5(0x11,0x12,0x14,0x18,0x14,0x12,0x11);
        case 'L': return GLYPH5(0x10,0x10,0x10,0x10,0x10,0x10,0x1F);
        case 'M': return GLYPH5(0x11,0x1B,0x15,0x15,0x11,0x11,0x11);
        case 'N': return GLYPH5(0x11,0x19,0x15,0x13,0x11,0x11,0x11);
        case 'O': return GLYPH5(0x0E,0x11,0x11,0x11,0x11,0x11,0x0E);
        case 'P': return GLYPH5(0x1E,0x11,0x11,0x1E,0x10,0x10,0x10);
        case 'Q': return GLYPH5(0x0E,0x11,0x11,0x11,0x15,0x12,0x0D);
        case 'R': return GLYPH5(0x1E,0x11,0x11,0x1E,0x14,0x12,0x11);
        case 'S': return GLYPH5(0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E);
        case 'T': return GLYPH5(0x1F,0x04,0x04,0x04,0x04,0x04,0x04);
        case 'U': return GLYPH5(0x11,0x11,0x11,0x11,0x11,0x11,0x0E);
        case 'V': return GLYPH5(0x11,0x11,0x11,0x11,0x11,0x0A,0x04);
        case 'W': return GLYPH5(0x11,0x11,0x11,0x15,0x15,0x1B,0x11);
        case 'X': return GLYPH5(0x11,0x11,0x0A,0x04,0x0A,0x11,0x11);
        case 'Y': return GLYPH5(0x11,0x11,0x0A,0x04,0x04,0x04,0x04);
        case 'Z': return GLYPH5(0x1F,0x01,0x02,0x04,0x08,0x10,0x1F);
        case '0': return GLYPH5(0x0E,0x11,0x13,0x15,0x19,0x11,0x0E);
        case '1': return GLYPH5(0x04,0x0C,0x04,0x04,0x04,0x04,0x0E);
        case '2': return GLYPH5(0x0E,0x11,0x01,0x02,0x04,0x08,0x1F);
        case '3': return GLYPH5(0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E);
        case '4': return GLYPH5(0x02,0x06,0x0A,0x12,0x1F,0x02,0x02);
        case '5': return GLYPH5(0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E);
        case '6': return GLYPH5(0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E);
        case '7': return GLYPH5(0x1F,0x01,0x02,0x04,0x08,0x08,0x08);
        case '8': return GLYPH5(0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E);
        case '9': return GLYPH5(0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E);
        case ':': return GLYPH5(0x00,0x04,0x04,0x00,0x04,0x04,0x00);
        case '-': return GLYPH5(0x00,0x00,0x00,0x1F,0x00,0x00,0x00);
        case '_': return GLYPH5(0x00,0x00,0x00,0x00,0x00,0x00,0x1F);
        case '/': return GLYPH5(0x01,0x02,0x02,0x04,0x08,0x08,0x10);
        case '%': return GLYPH5(0x19,0x19,0x02,0x04,0x08,0x13,0x13);
        case '.': return GLYPH5(0x00,0x00,0x00,0x00,0x00,0x0C,0x0C);
        case ' ': return GLYPH5(0x00,0x00,0x00,0x00,0x00,0x00,0x00);
        default:  return GLYPH5(0x00,0x00,0x1F,0x02,0x04,0x00,0x04);
    }
}

static void gc9a01_init_sequence()
{
    uint8_t d[14];
    write_cmd(0xEF);
    d[0] = 0x14; write_cmd_data(0xEB, d, 1);
    write_cmd(0xFE); write_cmd(0xEF);
    d[0] = 0x14; write_cmd_data(0xEB, d, 1);
    d[0] = 0x40; write_cmd_data(0x84, d, 1);
    d[0] = 0xFF; write_cmd_data(0x85, d, 1); write_cmd_data(0x86, d, 1); write_cmd_data(0x87, d, 1);
    d[0] = 0x0A; write_cmd_data(0x88, d, 1);
    d[0] = 0x21; write_cmd_data(0x89, d, 1);
    d[0] = 0x00; write_cmd_data(0x8A, d, 1);
    d[0] = 0x80; write_cmd_data(0x8B, d, 1);
    d[0] = 0x01; write_cmd_data(0x8C, d, 1); write_cmd_data(0x8D, d, 1);
    d[0] = 0xFF; write_cmd_data(0x8E, d, 1); write_cmd_data(0x8F, d, 1);
    d[0] = 0x00; d[1] = 0x20; write_cmd_data(0xB6, d, 2);
    d[0] = DISPLAY_MADCTL; write_cmd_data(0x36, d, 1);
    d[0] = 0x05; write_cmd_data(0x3A, d, 1);
    d[0] = 0x08; d[1] = 0x08; d[2] = 0x08; d[3] = 0x08; write_cmd_data(0x90, d, 4);
    d[0] = 0x06; write_cmd_data(0xBD, d, 1);
    d[0] = 0x00; write_cmd_data(0xBC, d, 1);
    d[0] = 0x60; d[1] = 0x01; d[2] = 0x04; write_cmd_data(0xFF, d, 3);
    d[0] = 0x13; write_cmd_data(0xC3, d, 1); write_cmd_data(0xC4, d, 1);
    d[0] = 0x22; write_cmd_data(0xC9, d, 1);
    d[0] = 0x11; write_cmd_data(0xBE, d, 1);
    d[0] = 0x10; d[1] = 0x0E; write_cmd_data(0xE1, d, 2);
    d[0] = 0x21; d[1] = 0x0C; d[2] = 0x02; write_cmd_data(0xDF, d, 3);
    const uint8_t f0[] = {0x45,0x09,0x08,0x08,0x26,0x2A};
    const uint8_t f1[] = {0x43,0x70,0x72,0x36,0x37,0x6F};
    write_cmd_data(0xF0, f0, sizeof(f0)); write_cmd_data(0xF1, f1, sizeof(f1));
    write_cmd_data(0xF2, f0, sizeof(f0)); write_cmd_data(0xF3, f1, sizeof(f1));
    d[0] = 0x1B; d[1] = 0x0B; write_cmd_data(0xED, d, 2);
    d[0] = 0x77; write_cmd_data(0xAE, d, 1);
    d[0] = 0x63; write_cmd_data(0xCD, d, 1);
    const uint8_t p70[] = {0x07,0x07,0x04,0x0E,0x0F,0x09,0x07,0x08,0x03};
    write_cmd_data(0x70, p70, sizeof(p70));
    d[0] = 0x34; write_cmd_data(0xE8, d, 1);
    const uint8_t p62[] = {0x18,0x0D,0x71,0xED,0x70,0x70,0x18,0x0F,0x71,0xEF,0x70,0x70};
    const uint8_t p63[] = {0x18,0x11,0x71,0xF1,0x70,0x70,0x18,0x13,0x71,0xF3,0x70,0x70};
    write_cmd_data(0x62, p62, sizeof(p62)); write_cmd_data(0x63, p63, sizeof(p63));
    const uint8_t p64[] = {0x28,0x29,0xF1,0x01,0xF1,0x00,0x07};
    write_cmd_data(0x64, p64, sizeof(p64));
    const uint8_t p66[] = {0x3C,0x00,0xCD,0x67,0x45,0x45,0x10,0x00,0x00,0x00};
    const uint8_t p67[] = {0x00,0x3C,0x00,0x00,0x00,0x01,0x54,0x10,0x32,0x98};
    write_cmd_data(0x66, p66, sizeof(p66)); write_cmd_data(0x67, p67, sizeof(p67));
    const uint8_t p74[] = {0x10,0x85,0x80,0x00,0x00,0x4E,0x00};
    write_cmd_data(0x74, p74, sizeof(p74));
    d[0] = 0x3E; d[1] = 0x07; write_cmd_data(0x98, d, 2);
    write_cmd(0x21);
    write_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    write_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
}

esp_err_t DisplayManager::initHardware()
{
    if (initialized) return ESP_OK;
    if (initFailed) return ESP_FAIL;

    gpio_config_t io;
    memset(&io, 0, sizeof(io));
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << DISPLAY_TFT_DC_PIN) | (1ULL << DISPLAY_TFT_RST_PIN);
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;

    spi_bus_config_t bus;
    memset(&bus, 0, sizeof(bus));
    bus.mosi_io_num = DISPLAY_TFT_MOSI_PIN;
    bus.miso_io_num = -1;
    bus.sclk_io_num = DISPLAY_TFT_SCLK_PIN;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = DISPLAY_TFT_WIDTH * 2 + 16;

    if (!s_bus_initialized) {
        err = spi_bus_initialize(DISPLAY_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            initFailed = true;
            ESP_LOGW(TAG, "SPI bus init failed: %s", esp_err_to_name(err));
            return err;
        }
        s_bus_initialized = true;
    }

    spi_device_interface_config_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.clock_speed_hz = DISPLAY_SPI_FREQUENCY;
    dev.mode = 0;
    dev.spics_io_num = DISPLAY_TFT_CS_PIN;
    dev.queue_size = 2;
    err = spi_bus_add_device(DISPLAY_SPI_HOST, &dev, &s_spi);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        initFailed = true;
        ESP_LOGW(TAG, "SPI device init failed: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_level((gpio_num_t)DISPLAY_TFT_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)DISPLAY_TFT_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    gc9a01_init_sequence();
    initialized = true;
    ESP_LOGI(TAG, "GC9A01 ready on VSPI SDA/MOSI=%d SCL/SCLK=%d CS=%d DC=%d RST=%d",
             DISPLAY_TFT_MOSI_PIN, DISPLAY_TFT_SCLK_PIN, DISPLAY_TFT_CS_PIN,
             DISPLAY_TFT_DC_PIN, DISPLAY_TFT_RST_PIN);
    return ESP_OK;
}

void DisplayManager::begin()
{
    display_config_t cfg;
    cfg.enabled = true;
    cfg.brightness = DEFAULT_DISPLAY_BRIGHTNESS;
    cfg.theme = DISPLAY_THEME_NEON_DARK;
    cfg.view_mode = DISPLAY_VIEW_AUTO;
    cfg.show_fps = true;
    cfg.show_wifi = true;
    begin(cfg);
}

void DisplayManager::begin(const display_config_t &cfg)
{
    config = cfg;
    frameInterval = DISPLAY_FRAME_MS;
    if (!config.enabled) return;
    if (initHardware() == ESP_OK) showBootScreen();
}

void DisplayManager::applyConfig(const display_config_t &cfg)
{
    config = cfg;
    frameInterval = DISPLAY_FRAME_MS;
    lastStaticHash = 0;
    lastDynamicMs = 0;
    if (!config.enabled) {
        setBacklight(0);
        return;
    }
    if (!initialized) initHardware();
    setBacklight(config.brightness);
}

void DisplayManager::setBacklight(uint8_t brightness)
{
    (void)brightness;
}

void DisplayManager::fillRect(int x, int y, int w, int h, uint16_t color)
{
    if (!initialized || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_TFT_WIDTH) w = DISPLAY_TFT_WIDTH - x;
    if (y + h > DISPLAY_TFT_HEIGHT) h = DISPLAY_TFT_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    uint16_t line[DISPLAY_TFT_WIDTH];
    uint16_t be = to_be(color);
    for (int i = 0; i < w; i++) line[i] = be;
    set_addr_window((uint16_t)x, (uint16_t)y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    for (int row = 0; row < h; row++) write_data(line, (size_t)w * 2);
}

void DisplayManager::fillScreen(uint16_t color)
{
    fillRect(0, 0, DISPLAY_TFT_WIDTH, DISPLAY_TFT_HEIGHT, color);
}

void DisplayManager::drawRect(int x, int y, int w, int h, uint16_t color)
{
    fillRect(x, y, w, 1, color);
    fillRect(x, y + h - 1, w, 1, color);
    fillRect(x, y, 1, h, color);
    fillRect(x + w - 1, y, 1, h, color);
}

void DisplayManager::drawLine(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        fillRect(x0, y0, 1, 1, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void DisplayManager::drawCircle(int cx, int cy, int r, uint16_t color, uint8_t thickness)
{
    for (uint8_t t = 0; t < thickness; t++) {
        int rr = r - t;
        int x = -rr, y = 0, err = 2 - 2 * rr;
        do {
            fillRect(cx - x, cy + y, 1, 1, color); fillRect(cx - y, cy - x, 1, 1, color);
            fillRect(cx + x, cy - y, 1, 1, color); fillRect(cx + y, cy + x, 1, 1, color);
            int e2 = err;
            if (e2 <= y) err += ++y * 2 + 1;
            if (e2 > x || err > y) err += ++x * 2 + 1;
        } while (x < 0);
    }
}

void DisplayManager::drawArc(int cx, int cy, int r, int startDeg, int endDeg,
                             uint16_t color, uint8_t thickness)
{
    if (endDeg < startDeg) return;
    for (int deg = startDeg; deg <= endDeg; deg += 3) {
        float rad = (float)deg * 3.14159265f / 180.0f;
        int x = cx + (int)roundf(cosf(rad) * r);
        int y = cy + (int)roundf(sinf(rad) * r);
        fillRect(x - (int)thickness / 2, y - (int)thickness / 2, thickness, thickness, color);
    }
}

void DisplayManager::drawText(int x, int y, const char *text, uint16_t color,
                              uint8_t scale, int maxWidth)
{
    if (!text || scale == 0) return;
    int startX = x;
    for (size_t i = 0; text[i]; i++) {
        if (maxWidth > 0 && x + (5 * scale) > startX + maxWidth) break;
        uint64_t g = glyph5x7(text[i]);
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                int bit = (6 - row) * 5 + (4 - col);
                if ((g >> bit) & 1) fillRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
        x += 6 * scale;
    }
}

void DisplayManager::drawTextCentered(int y, const char *text, uint16_t color, uint8_t scale)
{
    int w = text_width_px(text, scale);
    drawText((DISPLAY_TFT_WIDTH - w) / 2, y, text, color, scale);
}

void DisplayManager::drawTextCenteredFit(int y, const char *text, uint16_t color,
                                         uint8_t preferredScale, uint8_t minScale,
                                         int maxWidth)
{
    uint8_t scale = preferredScale;
    while (scale > minScale && text_width_px(text, scale) > maxWidth) scale--;
    int w = text_width_px(text, scale);
    if (w > maxWidth) w = maxWidth;
    drawText((DISPLAY_TFT_WIDTH - w) / 2, y, text, color, scale, maxWidth);
}

void DisplayManager::drawBar(int x, int y, int w, int h, uint8_t value, uint16_t color)
{
    drawRect(x, y, w, h, C.panel);
    fillRect(x + 2, y + 2, w - 4, h - 4, C.bg);
    int inner = (w - 4) * value / 255;
    fillRect(x + 2, y + 2, inner, h - 4, color);
}

void DisplayManager::drawMetricBar(int x, int y, int w, const char *label, uint8_t value,
                                   uint16_t color)
{
    (void)label;
    fillRect(x, y, w, 14, C.bg);
    drawBar(x, y, w, 14, value, color);
}

void DisplayManager::drawWifiIcon(int x, int y, bool connected)
{
    uint16_t col = connected ? C.green : C.red;
    fillRect(x, y, 24, 16, C.bg);
    fillRect(x + 2, y + 10, 4, 5, col);
    fillRect(x + 9, y + 6, 4, 9, connected ? col : C.panel);
    fillRect(x + 16, y + 2, 4, 13, connected ? col : C.panel);
}

void DisplayManager::drawTopStatus(const system_monitor_snapshot_t &system)
{
    fillRect(0, 0, DISPLAY_TFT_WIDTH, 44, C.bg);
    fillRect(38, 43, 164, 1, C.panel);

    if (config.show_wifi) {
        bool connected = system.wifi_clients > 0;
        drawWifiIcon(44, 20, connected);
        drawText(72, 21, connected ? "ON" : "OFF", connected ? C.green : C.red, 1, 40);
    }

    if (config.show_fps) {
        char line[16];
        snprintf(line, sizeof(line), "%uFPS", fps);
        drawText(132, 21, line, C.cyan, 1, 70);
    }
}

void DisplayManager::showBootScreen()
{
    if (!initialized) return;
    fillScreen(C.bg);
    drawTextCentered(50, "ESP32", C.cyan, 4);
    drawTextCenteredFit(92, "LED CONTROLLER", C.text, 2, 1, 170);
    drawTextCentered(160, "GC9A01 READY", C.orange, 2);

    drawRect(50, 128, 140, 16, C.panel);
    for (int step = 0; step <= 100; step += 4) {
        uint8_t value = (uint8_t)(step * 255 / 100);
        fillRect(52, 130, 136, 12, C.bg);
        fillRect(52, 130, 136 * value / 255, 12, step < 70 ? C.cyan : C.green);

        int scanX = 54 + ((step * 118) / 100);
        fillRect(scanX, 124, 8, 4, C.orange);
        if (scanX > 56) fillRect(scanX - 10, 124, 8, 4, C.bg);

        vTaskDelay(pdMS_TO_TICKS(55));
    }

    fillRect(44, 124, 152, 24, C.bg);
    drawBar(54, 132, 132, 12, 255, C.green);
    vTaskDelay(pdMS_TO_TICKS(650));
}

const char *DisplayManager::modeLabel(operating_mode_t mode) const
{
    switch (mode) {
        case OPERATING_MODE_REACTIVE:
        case OPERATING_MODE_REACTIVE_MATRIX:
            return "REACTIVE";
        case OPERATING_MODE_MATRIX:
            return "MATRIX";
        case OPERATING_MODE_NORMAL:
        default:
            return "BASIC";
    }
}

const char *DisplayManager::effectLabel(const app_config_t &state) const
{
    switch (state.operating_mode) {
        case OPERATING_MODE_REACTIVE:
            return config_manager_reactive_effect_to_string(state.active_reactive_effect);
        case OPERATING_MODE_MATRIX:
            return config_manager_matrix_effect_to_string(state.active_matrix_effect);
        case OPERATING_MODE_REACTIVE_MATRIX:
            return config_manager_rx_matrix_effect_to_string(state.active_rx_matrix_effect);
        case OPERATING_MODE_NORMAL:
        default:
            return config_manager_animation_to_string(state.animation);
    }
}

uint32_t DisplayManager::stateHash(const app_config_t &state, const audio_features_t &audio,
                                   const system_monitor_snapshot_t &system) const
{
    (void)audio;
    (void)system;
    uint32_t h = 2166136261u;
    const char *effect = effectLabel(state);
    for (size_t i = 0; effect && effect[i]; i++) h = (h ^ (uint8_t)effect[i]) * 16777619u;
    h = (h ^ state.operating_mode) * 16777619u;
    h = (h ^ state.brightness) * 16777619u;
    h = (h ^ state.animation_speed) * 16777619u;
    h = (h ^ state.audio.sensitivity) * 16777619u;
    h = (h ^ state.palette_id) * 16777619u;
    h = (h ^ state.random_normal.enabled) * 16777619u;
    h = (h ^ state.power) * 16777619u;
    h = (h ^ config.view_mode) * 16777619u;
    h = (h ^ config.show_fps) * 16777619u;
    h = (h ^ config.show_wifi) * 16777619u;
    return h;
}

void DisplayManager::drawStatusText(const app_config_t &state, const audio_features_t &audio,
                                    const system_monitor_snapshot_t &system)
{
    drawTopStatus(system);
    drawBar(48, 74, 144, 16, state.brightness, C.green);
    drawBar(48, 116, 144, 16, state.animation_speed, C.orange);
    drawBar(48, 158, 144, 16, audio.volume_8bit,
            audio.beat_detected ? C.orange : C.cyan);
    drawBar(66, 198, 108, 12, state.power ? 220 : 40, state.power ? C.green : C.red);
}

void DisplayManager::showBasicScreen(const app_config_t &state, const audio_features_t &audio,
                                     const system_monitor_snapshot_t &system)
{
    uint32_t h = stateHash(state, audio, system) ^ 0x5A5A0001u;
    bool layoutChanged = h != lastStaticHash;
    if (layoutChanged) {
        lastStaticHash = h;
        lastDynamicMs = 0;
        fillScreen(C.bg);
        drawStatusText(state, audio, system);
    }

    uint32_t now = now_ms();
    if (!layoutChanged && (uint32_t)(now - lastDynamicMs) < 250) return;
    lastDynamicMs = now;

    drawTopStatus(system);
    fillRect(48, 158, 144, 16, C.bg);
    drawBar(48, 158, 144, 16, audio.volume_8bit,
            audio.beat_detected ? C.orange : C.cyan);
}

void DisplayManager::drawCircularVu(uint8_t level, bool beat)
{
    (void)level;
    (void)beat;
}

void DisplayManager::drawSpectrumBars(const app_config_t &state, const audio_features_t &audio)
{
    const int bars = 16;
    const int baseY = 194;
    const int maxH = 142;
    const int startX = 34;
    const int gap = 2;
    const int bw = 9;
    for (int i = 0; i < bars; i++) {
        uint8_t v = (i < audio.band_count) ? smoothSpectrum[i] : audio.volume_8bit;
        if (state.operating_mode == OPERATING_MODE_REACTIVE) {
            if (state.active_reactive_effect == REACTIVE_EFFECT_BASS_HIT && i < 5) {
                int boosted = (int)v + audio.bass_level / 2;
                v = (uint8_t)(boosted > 255 ? 255 : boosted);
            } else if (state.active_reactive_effect == REACTIVE_EFFECT_PULSE ||
                       state.active_reactive_effect == REACTIVE_EFFECT_BEAT_FLASH) {
                int pulse = audio.beat_detected ? 36 : 0;
                int boosted = (int)v + pulse;
                v = (uint8_t)(boosted > 255 ? 255 : boosted);
            } else if (state.active_reactive_effect == REACTIVE_EFFECT_SPARK ||
                       state.active_reactive_effect == REACTIVE_EFFECT_COMET ||
                       state.active_reactive_effect == REACTIVE_EFFECT_CHASE) {
                int wave = ((i * 17 + visualFrame * 5) & 0x3F);
                int boosted = (int)v + wave / 3;
                v = (uint8_t)(boosted > 255 ? 255 : boosted);
            }
        }

        int h = 2 + (maxH * v / 255);
        if (h > spectrumPeaks[i]) spectrumPeaks[i] = (uint8_t)h;
        else if (spectrumPeaks[i] > 4) spectrumPeaks[i] -= 4;

        int x = startX + i * (bw + gap);
        int filled = 0;
        for (int y = baseY - 4; filled < h; y -= 6, filled += 6) {
            uint16_t col = (filled < 42) ? C.green : ((filled < 94) ? C.cyan : C.orange);
            if (audio.beat_detected && i >= 11) col = C.orange;
            fillRect(x, y, bw, 4, col);
        }

        int py = baseY - spectrumPeaks[i] - 3;
        if (py < 54) py = 54;
        fillRect(x, py, bw, 2, C.text);
    }
}

void DisplayManager::drawVuMeter(const audio_features_t &audio)
{
    const int x = 40;
    const int y = 58;
    const int w = 160;
    const int segH = 6;
    const int gap = 3;
    const int segments = 15;
    int active = smoothLevel * segments / 255;

    for (int i = 0; i < segments; i++) {
        int yy = y + (segments - 1 - i) * (segH + gap);
        uint16_t col = i < 6 ? C.green : (i < 12 ? C.cyan : C.orange);
        int inset = i < 8 ? i * 2 : (15 - i) * 2;
        if (inset < 0) inset = 0;
        fillRect(x + inset, yy, w - inset * 2, segH, i < active ? col : C.panel);
    }

    drawBar(48, 214, 144, 11, audio.beat_detected ? 255 : smoothBass, C.orange);
}

void DisplayManager::drawBeatPulse(bool beat, uint8_t level)
{
    (void)beat;
    (void)level;
}

void DisplayManager::drawWaveform(uint8_t level)
{
    waveform[waveformPos++ % sizeof(waveform)] = level;
    int ox = 50;
    int oy = 142;
    int prevX = ox;
    int prevY = oy - ((int)waveform[waveformPos % sizeof(waveform)] - 128) / 10;
    for (int i = 1; i < 64; i++) {
        uint8_t idx = (uint8_t)((waveformPos + i) % sizeof(waveform));
        int x = ox + i * 2;
        int y = oy - ((int)waveform[idx] - 128) / 10;
        drawLine(prevX, prevY, x, y, C.green);
        prevX = x;
        prevY = y;
    }
}

void DisplayManager::drawBassMidTreble(const audio_features_t &audio)
{
    drawBar(48, 58, 42, 12, audio.bass_level, C.green);
    drawBar(99, 58, 42, 12, audio.mid_level, C.cyan);
    drawBar(150, 58, 42, 12, audio.treble_level, C.purple);
}

void DisplayManager::showReactiveScreen(const app_config_t &state, const audio_features_t &audio,
                                        const system_monitor_snapshot_t &system)
{
    uint32_t h = stateHash(state, audio, system) ^ 0xA5A50002u;
    bool layoutChanged = h != lastStaticHash;
    if (layoutChanged) {
        lastStaticHash = h;
        lastDynamicMs = 0;
        memset(waveform, 128, sizeof(waveform));
        memset(spectrumPeaks, 0, sizeof(spectrumPeaks));
        waveformPos = 0;
        fillScreen(C.bg);
        beatRingActive = false;
    }

    visualFrame++;
    smoothLevel = smooth_u8(smoothLevel, audio.volume_8bit);
    smoothBass = smooth_u8(smoothBass, audio.bass_level);
    smoothMid = smooth_u8(smoothMid, audio.mid_level);
    smoothTreble = smooth_u8(smoothTreble, audio.treble_level);
    for (int i = 0; i < 16; i++) {
        uint8_t target = (i < audio.band_count) ? audio.spectrum_bands[i] : audio.volume_8bit;
        smoothSpectrum[i] = smooth_u8(smoothSpectrum[i], target);
    }

    audio_features_t displayAudio = audio;
    displayAudio.volume_8bit = smoothLevel;
    displayAudio.bass_level = smoothBass;
    displayAudio.mid_level = smoothMid;
    displayAudio.treble_level = smoothTreble;

    drawTopStatus(system);

    display_view_mode_t activeView = config.view_mode;
    if (activeView == DISPLAY_VIEW_AUTO) {
        if (state.operating_mode == OPERATING_MODE_REACTIVE &&
            state.active_reactive_effect == REACTIVE_EFFECT_VU_BAR) {
            activeView = DISPLAY_VIEW_VU_METER;
        } else {
            activeView = DISPLAY_VIEW_SPECTRUM;
        }
    }

    if (activeView == DISPLAY_VIEW_WAVEFORM) {
        fillRect(48, 52, 140, 136, C.bg);
        drawWaveform(smoothLevel);
    } else if (activeView == DISPLAY_VIEW_VU_METER) {
        fillRect(34, 50, 172, 148, C.bg);
        drawVuMeter(displayAudio);
    } else {
        fillRect(32, 50, 178, 148, C.bg);
        drawSpectrumBars(state, displayAudio);
    }

    beatRingActive = false;

    if (activeView != DISPLAY_VIEW_VU_METER) {
        fillRect(42, 211, 156, 15, C.bg);
        drawBar(42, 211, 156, 15, smoothLevel, audio.beat_detected ? C.orange : C.cyan);
    }
}

void DisplayManager::update(const app_config_t &state, const audio_features_t &audio,
                            const system_monitor_snapshot_t &system)
{
    if (!config.enabled) return;
    if (!initialized && initHardware() != ESP_OK) return;

    bool reactive = state.operating_mode == OPERATING_MODE_REACTIVE ||
                    state.operating_mode == OPERATING_MODE_REACTIVE_MATRIX;
    bool visualizer = reactive || config.view_mode == DISPLAY_VIEW_SPECTRUM ||
                      config.view_mode == DISPLAY_VIEW_VU_METER ||
                      config.view_mode == DISPLAY_VIEW_WAVEFORM;
    uint16_t targetInterval = visualizer ? DISPLAY_FRAME_MS : 150;

    uint32_t now = now_ms();
    if ((uint32_t)(now - lastUpdate) < targetInterval) return;
    lastUpdate = now;
    frameInterval = targetInterval;

    if (fpsWindowMs == 0) fpsWindowMs = now;
    fpsFrames++;
    if ((uint32_t)(now - fpsWindowMs) >= 1000) {
        fps = fpsFrames;
        fpsFrames = 0;
        fpsWindowMs = now;
    }
    switch (config.view_mode) {
        case DISPLAY_VIEW_STATUS:
            showBasicScreen(state, audio, system);
            break;
        case DISPLAY_VIEW_SPECTRUM:
        case DISPLAY_VIEW_VU_METER:
        case DISPLAY_VIEW_WAVEFORM:
            showReactiveScreen(state, audio, system);
            break;
        case DISPLAY_VIEW_AUTO:
        default:
            if (reactive) showReactiveScreen(state, audio, system);
            else showBasicScreen(state, audio, system);
            break;
    }
}

extern "C" esp_err_t display_manager_begin(const display_config_t *config)
{
    if (config) s_display.begin(*config);
    else s_display.begin();
    return ESP_OK;
}

extern "C" void display_manager_apply_config(const display_config_t *config)
{
    if (config) s_display.applyConfig(*config);
}

extern "C" void display_manager_update(const app_config_t *state, const audio_features_t *audio,
                                       const system_monitor_snapshot_t *system)
{
    if (!state || !audio || !system) return;
    s_display.update(*state, *audio, *system);
}

extern "C" void display_manager_show_boot_screen(void)
{
    s_display.showBootScreen();
}

extern "C" void display_manager_set_backlight(uint8_t brightness)
{
    s_display.setBacklight(brightness);
}
