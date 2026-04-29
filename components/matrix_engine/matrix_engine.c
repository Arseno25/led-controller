#include "matrix_engine.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "mx";

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static matrix_config_t s_mx;
static app_config_t    s_app;
static bool            s_app_set = false;

/* Per-effect persistent state */
static struct {
    uint32_t frame;
    /* matrix rain */
    int16_t  drops[MATRIX_MAX_W];
    /* matrix fire heat map (height * width) */
    uint8_t  heat[MATRIX_MAX_W * MATRIX_MAX_H];
    /* plasma phase */
    float    plasma_t;
    /* ripple */
    float    ripple_cx, ripple_cy, ripple_r;
    bool     ripple_active;
    /* spark field positions */
    uint16_t spark_pos[MATRIX_MAX_W * MATRIX_MAX_H];
    uint8_t  spark_age[MATRIX_MAX_W * MATRIX_MAX_H];
    uint16_t spark_n;
    uint8_t  peaks[MATRIX_MAX_W];
} M;

esp_err_t matrix_engine_init(const matrix_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    s_mx = *cfg;
    memset(&M, 0, sizeof(M));
    for (int i = 0; i < MATRIX_MAX_W; i++) M.drops[i] = -(int16_t)(esp_random() % 16);
    ESP_LOGI(TAG, "init %ux%u layout=%d origin=%d",
             cfg->width, cfg->height, cfg->layout, cfg->origin);
    return ESP_OK;
}

esp_err_t matrix_engine_set_config(const matrix_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    s_mx = *cfg;
    matrix_engine_reset();
    return ESP_OK;
}

esp_err_t matrix_engine_set_params(const app_config_t *app)
{
    if (!app) return ESP_ERR_INVALID_ARG;
    s_app = *app;
    s_app_set = true;
    return ESP_OK;
}

uint16_t matrix_engine_get_width(void)  { return s_mx.rotate_90 ? s_mx.height : s_mx.width; }
uint16_t matrix_engine_get_height(void) { return s_mx.rotate_90 ? s_mx.width  : s_mx.height; }

uint16_t matrix_engine_xy_to_index(uint16_t x, uint16_t y)
{
    const uint16_t phys_w = s_mx.width;
    const uint16_t phys_h = s_mx.height;
    if (phys_w == 0 || phys_h == 0) return 0;

    uint16_t W = phys_w;
    uint16_t H = phys_h;
    if (s_mx.rotate_90) {
        uint16_t px = y;
        uint16_t py = (phys_h > 0) ? (phys_h - 1 - x) : 0;
        x = px;
        y = py;
        W = phys_w;
        H = phys_h;
    }

    if (s_mx.reverse_x) x = (W > 0) ? (W - 1 - x) : 0;
    if (s_mx.reverse_y) y = (H > 0) ? (H - 1 - y) : 0;

    /* origin */
    switch (s_mx.origin) {
        case MATRIX_ORIGIN_TOP_RIGHT:    x = (W > 0) ? (W - 1 - x) : 0; break;
        case MATRIX_ORIGIN_BOTTOM_LEFT:  y = (H > 0) ? (H - 1 - y) : 0; break;
        case MATRIX_ORIGIN_BOTTOM_RIGHT: x = (W > 0) ? (W - 1 - x) : 0; y = (H > 0) ? (H - 1 - y) : 0; break;
        default: break;
    }

    if (x >= W) x = W - 1;
    if (y >= H) y = H - 1;

    uint16_t idx;
    if (s_mx.layout == MATRIX_LAYOUT_SERPENTINE && (y & 1)) {
        idx = y * W + (W - 1 - x);
    } else {
        idx = y * W + x;
    }
    return idx;
}

esp_err_t matrix_engine_reset(void)
{
    memset(&M, 0, sizeof(M));
    for (int i = 0; i < MATRIX_MAX_W; i++) M.drops[i] = -(int16_t)(esp_random() % 16);
    return ESP_OK;
}

/* ---- helpers ---- */

static uint8_t sc8(uint8_t v, uint8_t s) { return (uint8_t)(((uint16_t)v * s) / 255); }
static led_color_t dim_c(led_color_t c, uint8_t d) {
    return (led_color_t){sc8(c.r,d), sc8(c.g,d), sc8(c.b,d), sc8(c.w,d)};
}

static void hsv2rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    while (h >= 360) h -= 360;
    while (h < 0) h += 360;
    int i = (int)(h / 60); float f = h/60 - i, p = v*(1-s), q = v*(1-s*f), t = v*(1-s*(1-f));
    float rf,gf,bf;
    switch (i) {
        case 0: rf=v;gf=t;bf=p; break; case 1: rf=q;gf=v;bf=p; break;
        case 2: rf=p;gf=v;bf=t; break; case 3: rf=p;gf=q;bf=v; break;
        case 4: rf=t;gf=p;bf=v; break; default: rf=v;gf=p;bf=q; break;
    }
    *r=(uint8_t)(rf*255); *g=(uint8_t)(gf*255); *b=(uint8_t)(bf*255);
}

static void put(uint16_t x, uint16_t y, led_color_t c)
{
    if (x >= matrix_engine_get_width() || y >= matrix_engine_get_height()) return;
    uint16_t idx = matrix_engine_xy_to_index(x, y);
    if (idx < led_driver_get_count()) led_driver_set_pixel(idx, c);
}

static void fill_bg(led_color_t bg)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    for (uint16_t y = 0; y < H; y++)
        for (uint16_t x = 0; x < W; x++) put(x, y, bg);
}

static led_color_t primary_color(void)
{
    return s_app_set ? (led_color_t){s_app.color_r, s_app.color_g, s_app.color_b, s_app.color_w}
                     : (led_color_t){0, 200, 255, 0};
}

static led_color_t secondary_color(void)
{
    return s_app_set ? (led_color_t){s_app.sec_r, s_app.sec_g, s_app.sec_b, s_app.sec_w}
                     : (led_color_t){255, 120, 0, 0};
}

static uint8_t spectrum_value_for_col(const audio_features_t *f, uint16_t x, uint16_t W)
{
    if (!f || W == 0) return 0;
    uint8_t bands = f->band_count > 0 ? f->band_count : AUDIO_SPECTRUM_BANDS_MAX;
    if (bands > AUDIO_SPECTRUM_BANDS_MAX) bands = AUDIO_SPECTRUM_BANDS_MAX;
    uint8_t b = (uint8_t)((uint32_t)x * bands / W);
    if (b >= bands) b = bands - 1;
    return f->spectrum_bands[b];
}

static led_color_t spectrum_hue(float hue, uint8_t value)
{
    uint8_t r, g, b;
    hsv2rgb(hue, 1.0f, (float)value / 255.0f, &r, &g, &b);
    return (led_color_t){r, g, b, 0};
}

/* ============ NORMAL MATRIX EFFECTS ============ */

static void mx_rain(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    uint8_t fade = s_app_set && s_app.fade_amount > 0 ? s_app.fade_amount : 60;
    /* Fade existing pixels */
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            uint16_t idx = matrix_engine_xy_to_index(x, y);
            if (idx >= led_driver_get_count()) continue;
            /* approximate fade by re-setting black at chance — simpler: just darken via scaled draw */
            led_color_t blank = {0,0,0,0};
            (void)blank;
            (void)fade;
            /* leave as-is; new draw overwrites */
        }
    }
    fill_bg((led_color_t){0,0,0,0});

    led_color_t green = {0,255,0,0};
    for (uint16_t x = 0; x < W && x < MATRIX_MAX_W; x++) {
        if ((M.frame % 2) == 0) M.drops[x]++;
        if (M.drops[x] > (int16_t)(H + 6)) {
            M.drops[x] = -(int16_t)(esp_random() % 12);
        }
        for (uint16_t y = 0; y < H; y++) {
            int16_t d = M.drops[x] - (int16_t)y;
            if (d == 0)            put(x, y, (led_color_t){180,255,180,0});
            else if (d > 0 && d < 8) put(x, y, dim_c(green, 255 - d * 30));
        }
    }
    M.frame++;
}

static void mx_rainbow(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    float speed = s_app_set ? (float)s_app.animation_speed : 80.0f;
    M.plasma_t += speed * 0.001f;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float h = fmodf(((float)x / W) * 360.0f + M.plasma_t * 60.0f, 360.0f);
            uint8_t r,g,b; hsv2rgb(h, 1, 1, &r, &g, &b);
            put(x, y, (led_color_t){r,g,b,0});
        }
    }
}

static void mx_fire(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    if (W > MATRIX_MAX_W) W = MATRIX_MAX_W;
    if (H > MATRIX_MAX_H) H = MATRIX_MAX_H;

    uint8_t cooling = s_app_set && s_app.cooling ? s_app.cooling : 55;
    uint8_t sparking = s_app_set && s_app.sparking ? s_app.sparking : 120;

    /* Cool every cell */
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            uint8_t c = M.heat[y * W + x];
            uint8_t cd = (uint8_t)((esp_random() % 8) * cooling / 100);
            M.heat[y * W + x] = c > cd ? c - cd : 0;
        }
    }
    /* Drift up */
    for (int y = H - 1; y >= 2; y--) {
        for (uint16_t x = 0; x < W; x++) {
            uint16_t a = M.heat[(y-1) * W + x];
            uint16_t b = M.heat[(y-2) * W + x];
            M.heat[y * W + x] = (uint8_t)((a + b) / 2);
        }
    }
    /* Spark at bottom */
    for (uint16_t x = 0; x < W; x++) {
        if ((esp_random() % 256) < sparking) {
            uint16_t i = x;  /* bottom row */
            M.heat[i] = (uint8_t)(160 + (esp_random() % 95));
        }
    }
    /* Map heat -> color, draw flipped (fire rises) */
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            uint8_t h = M.heat[y * W + x];
            led_color_t c;
            if (h < 85)        c = (led_color_t){h*3, 0, 0, 0};
            else if (h < 170)  c = (led_color_t){255, (h-85)*3, 0, 0};
            else               c = (led_color_t){255, 255, (h-170)*3, 0};
            put(x, H - 1 - y, c);
        }
    }
}

static void mx_plasma(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    float speed = s_app_set ? (float)s_app.animation_speed : 80.0f;
    M.plasma_t += speed * 0.0006f;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float v = sinf((float)x * 0.4f + M.plasma_t)
                    + sinf((float)y * 0.5f + M.plasma_t * 1.2f)
                    + sinf(((float)x + (float)y) * 0.3f + M.plasma_t * 0.7f);
            float h = fmodf((v + 3.0f) * 60.0f, 360.0f);
            uint8_t r,g,b; hsv2rgb(h, 1, 1, &r, &g, &b);
            put(x, y, (led_color_t){r,g,b,0});
        }
    }
}

static void mx_lava(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    float speed = s_app_set ? (float)s_app.animation_speed : 80.0f;
    M.plasma_t += speed * 0.0007f;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float v = sinf((float)x * 0.35f + M.plasma_t)
                    + sinf((float)y * 0.55f + M.plasma_t * 1.5f);
            uint8_t heat = (uint8_t)((v + 2.0f) * 63.0f);
            led_color_t c = heat < 85 ? (led_color_t){heat * 3, 0, 0, 0}
                            : heat < 170 ? (led_color_t){255, (heat - 85) * 3, 0, 0}
                                         : (led_color_t){255, 255, (heat - 170) * 3, 0};
            put(x, y, c);
        }
    }
    (void)dt;
}

static void mx_aurora(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    float speed = s_app_set ? (float)s_app.animation_speed : 80.0f;
    M.plasma_t += speed * 0.00045f;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float wave = sinf((float)x * 0.38f + M.plasma_t)
                       + sinf((float)y * 0.22f + M.plasma_t * 0.7f);
            float h = 130.0f + wave * 45.0f + ((float)y / (float)H) * 60.0f;
            uint8_t r,g,b; hsv2rgb(h, 0.75f, 0.85f, &r, &g, &b);
            put(x, y, (led_color_t){r,g,b,0});
        }
    }
    (void)dt;
}

static void mx_ripple(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    if (!M.ripple_active || M.ripple_r > (float)(W > H ? W : H)) {
        M.ripple_cx = (float)(esp_random() % W);
        M.ripple_cy = (float)(esp_random() % H);
        M.ripple_r = 0.0f;
        M.ripple_active = true;
    }
    fill_bg((led_color_t){0,0,0,0});
    led_color_t pri = primary_color();
    led_color_t sec = secondary_color();
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float dx = (float)x - M.ripple_cx;
            float dy = (float)y - M.ripple_cy;
            float d = sqrtf(dx * dx + dy * dy);
            float diff = fabsf(d - M.ripple_r);
            if (diff < 2.0f) {
                put(x, y, dim_c(diff < 0.75f ? pri : sec, (uint8_t)((2.0f - diff) * 120.0f)));
            }
        }
    }
    M.ripple_r += 0.35f + (s_app_set ? s_app.animation_speed : 80) / 255.0f;
    (void)dt;
}

static void mx_starfield(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    uint16_t pixels = W * H;
    uint16_t stars = 2 + ((uint32_t)(s_app_set ? s_app.density : 30) * pixels) / 350;
    for (uint16_t i = 0; i < stars; i++) {
        uint16_t x = esp_random() % W;
        uint16_t y = esp_random() % H;
        uint8_t b = 96 + (esp_random() % 160);
        put(x, y, (led_color_t){b,b,b,0});
    }
    (void)dt;
}

static void mx_waterfall(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    float speed = s_app_set ? (float)s_app.animation_speed : 80.0f;
    M.plasma_t += speed * 0.001f;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float v = sinf((float)x * 0.55f + M.plasma_t)
                    + sinf((float)(y + M.frame) * 0.35f);
            uint8_t b = (uint8_t)((v + 2.0f) * 63.0f);
            put(x, y, (led_color_t){0, (uint8_t)(b / 2), b, 0});
        }
    }
    (void)dt;
}

static void mx_blocks(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    uint8_t block = s_app_set && s_app.animation_size ? s_app.animation_size : 4;
    if (block < 2) block = 2;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            uint16_t bx = x / block;
            uint16_t by = y / block;
            float h = fmodf((float)((bx * 47 + by * 83 + M.frame) % 360), 360.0f);
            uint8_t r,g,b; hsv2rgb(h, 0.9f, 0.85f, &r, &g, &b);
            put(x, y, (led_color_t){r,g,b,0});
        }
    }
    (void)dt;
}

static void mx_noise_flow(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    float speed = s_app_set ? (float)s_app.animation_speed : 80.0f;
    M.plasma_t += speed * 0.0009f;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float v = sinf((float)x * 0.23f + M.plasma_t)
                    * cosf((float)y * 0.31f - M.plasma_t * 1.4f)
                    + sinf(((float)x + y) * 0.19f + M.plasma_t * 0.6f);
            uint8_t r,g,b; hsv2rgb((v + 2.0f) * 100.0f, 0.85f, 0.9f, &r, &g, &b);
            put(x, y, (led_color_t){r,g,b,0});
        }
    }
    (void)dt;
}

static void mx_spiral(uint32_t dt)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    float cx = (float)(W - 1) * 0.5f;
    float cy = (float)(H - 1) * 0.5f;
    float speed = s_app_set ? (float)s_app.animation_speed : 80.0f;
    M.plasma_t += speed * 0.0008f;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float angle = atan2f(dy, dx);
            float dist = sqrtf(dx * dx + dy * dy);
            float h = fmodf((angle + (float)M_PI) * 57.3f + dist * 26.0f + M.plasma_t * 120.0f, 360.0f);
            uint8_t r,g,b; hsv2rgb(h, 1.0f, 0.9f, &r, &g, &b);
            put(x, y, (led_color_t){r,g,b,0});
        }
    }
    (void)dt;
}

esp_err_t matrix_engine_render_normal(matrix_effect_t effect, uint32_t delta_ms)
{
    if (!s_mx.enabled || (s_app_set && !s_app.power)) {
        led_driver_clear();
        return ESP_OK;
    }
    led_driver_set_brightness(s_app_set ? s_app.brightness : 128);
    switch (effect) {
        case MATRIX_EFFECT_RAIN:    mx_rain(delta_ms);    break;
        case MATRIX_EFFECT_RAINBOW: mx_rainbow(delta_ms); break;
        case MATRIX_EFFECT_FIRE:    mx_fire(delta_ms);    break;
        case MATRIX_EFFECT_PLASMA:  mx_plasma(delta_ms);  break;
        case MATRIX_EFFECT_LAVA:    mx_lava(delta_ms);    break;
        case MATRIX_EFFECT_AURORA:  mx_aurora(delta_ms);  break;
        case MATRIX_EFFECT_RIPPLE:  mx_ripple(delta_ms);  break;
        case MATRIX_EFFECT_STARFIELD: mx_starfield(delta_ms); break;
        case MATRIX_EFFECT_WATERFALL: mx_waterfall(delta_ms); break;
        case MATRIX_EFFECT_BLOCKS:  mx_blocks(delta_ms);  break;
        case MATRIX_EFFECT_NOISE_FLOW: mx_noise_flow(delta_ms); break;
        case MATRIX_EFFECT_SPIRAL:  mx_spiral(delta_ms);  break;
        default:                    mx_rain(delta_ms);    break;
    }
    M.frame++;
    return ESP_OK;
}

/* ============ REACTIVE MATRIX EFFECTS ============ */

static void rx_spectrum_bars(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    if (!f) return;
    led_color_t primary = s_app_set
        ? (led_color_t){s_app.color_r, s_app.color_g, s_app.color_b, s_app.color_w}
        : (led_color_t){0, 255, 0, 0};

    uint8_t bands = f->band_count > 0 ? f->band_count : 8;
    for (uint16_t x = 0; x < W; x++) {
        uint8_t b = (uint8_t)((uint32_t)x * bands / W);
        if (b >= AUDIO_SPECTRUM_BANDS_MAX) b = AUDIO_SPECTRUM_BANDS_MAX - 1;
        uint8_t v = f->spectrum_bands[b];
        uint16_t h = (uint16_t)((uint32_t)v * H / 255);
        for (uint16_t y = 0; y < h; y++) {
            uint8_t r,g,bl;
            float hue = ((float)y / H) * 270.0f;  /* green->yellow->red */
            hsv2rgb(120 - hue * 0.4f, 1, 1, &r, &g, &bl);
            put(x, H - 1 - y, (led_color_t){r,g,bl,0});
        }
        (void)primary;
    }
}

static void rx_spectrum_rainbow(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    if (!f || W == 0 || H == 0) return;

    float drift = (float)(M.frame % 360);
    for (uint16_t x = 0; x < W; x++) {
        uint8_t v = spectrum_value_for_col(f, x, W);
        uint16_t bar_h = (uint16_t)((uint32_t)v * H / 255);
        float hue = fmodf(((float)x * 360.0f / (float)W) + drift, 360.0f);
        for (uint16_t y = 0; y < bar_h; y++) {
            uint8_t level = (uint8_t)(120 + ((uint32_t)y * 135 / H));
            put(x, H - 1 - y, spectrum_hue(hue, level));
        }
    }
}

static void rx_spectrum_mirror(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    if (!f || W == 0 || H == 0) return;

    uint16_t center_lo = (H - 1) / 2;
    uint16_t center_hi = H / 2;
    uint16_t half = (H + 1) / 2;
    for (uint16_t x = 0; x < W; x++) {
        uint8_t v = spectrum_value_for_col(f, x, W);
        uint16_t span = (uint16_t)((uint32_t)v * half / 255);
        float hue = fmodf(170.0f + ((float)x * 180.0f / (float)W) + (float)(M.frame % 120), 360.0f);
        for (uint16_t y = 0; y < span; y++) {
            uint8_t level = (uint8_t)(100 + ((uint32_t)y * 155 / half));
            if (center_hi + y < H) put(x, center_hi + y, spectrum_hue(hue, level));
            if (center_lo >= y) put(x, center_lo - y, spectrum_hue(hue, level));
        }
    }
}

static void rx_spectrum_peaks(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    if (!f || W == 0 || H == 0) return;

    for (uint16_t x = 0; x < W && x < MATRIX_MAX_W; x++) {
        uint8_t v = spectrum_value_for_col(f, x, W);
        uint8_t bar_h = (uint8_t)((uint32_t)v * H / 255);
        if (bar_h > M.peaks[x]) {
            M.peaks[x] = bar_h;
        } else if ((M.frame & 1) == 0 && M.peaks[x] > 0) {
            M.peaks[x]--;
        }

        for (uint16_t y = 0; y < bar_h; y++) {
            float hue = 118.0f - ((float)y * 78.0f / (float)H);
            put(x, H - 1 - y, spectrum_hue(hue, 210));
        }

        if (M.peaks[x] > 0) {
            uint16_t peak_y = H - M.peaks[x];
            put(x, peak_y, (led_color_t){255, 230, 160, 0});
        }
    }
}

static void rx_spectrum_waterfall(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    if (!f || W == 0 || H == 0) { fill_bg((led_color_t){0,0,0,0}); return; }
    if (W > MATRIX_MAX_W) W = MATRIX_MAX_W;
    if (H > MATRIX_MAX_H) H = MATRIX_MAX_H;

    for (int y = (int)H - 1; y > 0; y--) {
        for (uint16_t x = 0; x < W; x++) {
            M.heat[y * MATRIX_MAX_W + x] = M.heat[(y - 1) * MATRIX_MAX_W + x];
        }
    }
    for (uint16_t x = 0; x < W; x++) {
        M.heat[x] = spectrum_value_for_col(f, x, W);
    }

    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            uint8_t v = M.heat[y * MATRIX_MAX_W + x];
            float hue = 230.0f - ((float)v * 190.0f / 255.0f);
            put(x, y, spectrum_hue(hue, v));
        }
    }
}

static void rx_segment_vu(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    if (!f || W == 0 || H == 0) return;

    led_color_t pri = primary_color();
    led_color_t sec = secondary_color();
    for (uint16_t x = 0; x < W; x++) {
        uint8_t v = spectrum_value_for_col(f, x, W);
        uint16_t bar_h = (uint16_t)((uint32_t)v * H / 255);
        bool flip = (s_mx.layout == MATRIX_LAYOUT_SERPENTINE) && (x & 1);
        for (uint16_t y = 0; y < bar_h; y++) {
            uint8_t mix = H > 1 ? (uint8_t)((uint32_t)y * 255 / (H - 1)) : 0;
            led_color_t c = (led_color_t){
                (uint8_t)(((uint16_t)pri.r * (255 - mix) + (uint16_t)sec.r * mix) / 255),
                (uint8_t)(((uint16_t)pri.g * (255 - mix) + (uint16_t)sec.g * mix) / 255),
                (uint8_t)(((uint16_t)pri.b * (255 - mix) + (uint16_t)sec.b * mix) / 255),
                (uint8_t)(((uint16_t)pri.w * (255 - mix) + (uint16_t)sec.w * mix) / 255),
            };
            uint16_t yy = flip ? y : (H - 1 - y);
            put(x, yy, c);
        }
    }
}

static void rx_center_vu(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    if (!f) return;
    uint8_t v = f->volume_8bit;
    uint16_t half = W / 2;
    uint16_t fill = (uint16_t)((uint32_t)v * half / 255);
    led_color_t primary = s_app_set
        ? (led_color_t){s_app.color_r, s_app.color_g, s_app.color_b, s_app.color_w}
        : (led_color_t){255, 80, 0, 0};
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t i = 0; i < fill; i++) {
            put(half + i, y, primary);
            if (half >= 1 + i) put(half - 1 - i, y, primary);
        }
    }
}

static void rx_bass_pulse(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    if (!f) { fill_bg((led_color_t){0,0,0,0}); return; }
    uint8_t bass = f->bass_level;
    led_color_t primary = s_app_set
        ? (led_color_t){s_app.color_r, s_app.color_g, s_app.color_b, s_app.color_w}
        : (led_color_t){255, 0, 80, 0};
    for (uint16_t y = 0; y < H; y++)
        for (uint16_t x = 0; x < W; x++)
            put(x, y, dim_c(primary, bass));
}

static void rx_audio_ripple(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    if (!f) return;

    if (f->beat_detected || f->onset_detected) {
        M.ripple_cx = (float)(esp_random() % W);
        M.ripple_cy = (float)(esp_random() % H);
        M.ripple_r  = 0;
        M.ripple_active = true;
    }
    if (M.ripple_active) {
        M.ripple_r += 0.7f;
        led_color_t primary = s_app_set
            ? (led_color_t){s_app.color_r, s_app.color_g, s_app.color_b, s_app.color_w}
            : (led_color_t){0, 200, 255, 0};
        for (uint16_t y = 0; y < H; y++) {
            for (uint16_t x = 0; x < W; x++) {
                float dx = (float)x - M.ripple_cx;
                float dy = (float)y - M.ripple_cy;
                float d = sqrtf(dx*dx + dy*dy);
                float diff = fabsf(d - M.ripple_r);
                if (diff < 1.5f) {
                    uint8_t b = (uint8_t)((1.5f - diff) / 1.5f * 255);
                    put(x, y, dim_c(primary, b));
                }
            }
        }
        if (M.ripple_r > (W > H ? W : H)) M.ripple_active = false;
    }
}

static void rx_beat_flash(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    if (!f) { fill_bg((led_color_t){0,0,0,0}); return; }
    led_color_t primary = s_app_set
        ? (led_color_t){s_app.color_r, s_app.color_g, s_app.color_b, s_app.color_w}
        : (led_color_t){255, 255, 255, 0};
    if (f->beat_detected) {
        for (uint16_t y = 0; y < H; y++)
            for (uint16_t x = 0; x < W; x++) put(x, y, primary);
    } else {
        for (uint16_t y = 0; y < H; y++)
            for (uint16_t x = 0; x < W; x++)
                put(x, y, dim_c(primary, f->volume_8bit / 4));
    }
}

static void rx_spark_field(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    fill_bg((led_color_t){0,0,0,0});
    if (!f) return;

    uint8_t v = f->volume_8bit;
    uint16_t want = (uint16_t)((uint32_t)v * (W * H) / 512);
    led_color_t primary = s_app_set
        ? (led_color_t){s_app.color_r, s_app.color_g, s_app.color_b, s_app.color_w}
        : (led_color_t){255, 180, 0, 0};

    for (uint16_t i = 0; i < want; i++) {
        uint16_t x = esp_random() % W;
        uint16_t y = esp_random() % H;
        put(x, y, primary);
    }
}

static void rx_audio_plasma(const audio_features_t *f)
{
    uint8_t vol = f ? f->volume_8bit : 0;
    float saved_speed = s_app_set ? s_app.animation_speed : 80.0f;
    M.plasma_t += (float)vol * 0.006f;
    (void)saved_speed;
    mx_plasma(0);
}

static void rx_fire_eq(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    if (!f) { fill_bg((led_color_t){0,0,0,0}); return; }
    uint8_t bands = f->band_count ? f->band_count : 8;
    if (bands > AUDIO_SPECTRUM_BANDS_MAX) bands = AUDIO_SPECTRUM_BANDS_MAX;
    for (uint16_t x = 0; x < W; x++) {
        uint8_t band = (uint8_t)((uint32_t)x * bands / W);
        if (band >= bands) band = bands - 1;
        uint8_t energy = f->spectrum_bands[band];
        for (uint16_t y = 0; y < H; y++) {
            uint8_t heat = (uint8_t)((uint32_t)energy * (H - y) / H);
            led_color_t c = heat < 85 ? (led_color_t){heat * 3, 0, 0, 0}
                            : heat < 170 ? (led_color_t){255, (heat - 85) * 3, 0, 0}
                                         : (led_color_t){255, 255, (heat - 170) * 3, 0};
            put(x, H - 1 - y, c);
        }
    }
}

static void rx_bass_tunnel(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    uint8_t bass = f ? f->bass_level : 0;
    float cx = (float)(W - 1) * 0.5f;
    float cy = (float)(H - 1) * 0.5f;
    M.plasma_t += 0.04f + (float)bass * 0.0009f;
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float d = sqrtf(dx * dx + dy * dy);
            float ring = sinf(d * 1.25f - M.plasma_t * 3.0f);
            uint8_t v = (uint8_t)((ring + 1.0f) * 0.5f * bass);
            uint8_t r,g,b; hsv2rgb(190.0f + bass * 0.45f, 1.0f, (float)v / 255.0f, &r, &g, &b);
            put(x, y, (led_color_t){r,g,b,0});
        }
    }
}

static void rx_treble_rain(const audio_features_t *f)
{
    uint8_t treble = f ? f->treble_level : 0;
    mx_rain(0);
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    uint16_t sparks = ((uint32_t)treble * W) / 180;
    led_color_t c = {180, 220, 255, 0};
    for (uint16_t i = 0; i < sparks; i++) {
        put(esp_random() % W, esp_random() % H, c);
    }
}

static void rx_mid_wave(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    uint8_t mid = f ? f->mid_level : 0;
    fill_bg((led_color_t){0,0,0,0});
    led_color_t pri = primary_color();
    float amp = 1.0f + ((float)mid / 255.0f) * ((float)H * 0.45f);
    M.plasma_t += 0.07f + mid * 0.0007f;
    for (uint16_t x = 0; x < W; x++) {
        float y_f = ((float)H * 0.5f) + sinf((float)x * 0.45f + M.plasma_t) * amp;
        int y0 = (int)y_f;
        for (int dy = -1; dy <= 1; dy++) {
            int y = y0 + dy;
            if (y >= 0 && y < (int)H) put(x, (uint16_t)y, dim_c(pri, (uint8_t)(220 - abs(dy) * 70)));
        }
    }
}

static void rx_audio_blocks(const audio_features_t *f)
{
    uint16_t W = matrix_engine_get_width(), H = matrix_engine_get_height();
    uint8_t vol = f ? f->volume_8bit : 0;
    uint8_t block = 2 + (vol / 48);
    for (uint16_t y = 0; y < H; y++) {
        for (uint16_t x = 0; x < W; x++) {
            uint16_t bx = x / block;
            uint16_t by = y / block;
            uint8_t band = (uint8_t)((bx + by) % AUDIO_SPECTRUM_BANDS_MAX);
            uint8_t v = f ? f->spectrum_bands[band] : vol;
            uint8_t r,g,b; hsv2rgb((float)(band * 24 + M.frame), 1.0f, (float)v / 255.0f, &r, &g, &b);
            put(x, y, (led_color_t){r,g,b,0});
        }
    }
}

static void rx_reactive_aurora(const audio_features_t *f)
{
    uint8_t vol = f ? f->volume_8bit : 0;
    M.plasma_t += (float)vol * 0.003f;
    mx_aurora(0);
}

static void rx_reactive_spiral(const audio_features_t *f)
{
    uint8_t vol = f ? f->volume_8bit : 0;
    M.plasma_t += (float)vol * 0.004f;
    mx_spiral(0);
}

esp_err_t matrix_engine_render_reactive(rx_matrix_effect_t effect,
                                        const audio_features_t *feat,
                                        uint32_t delta_ms)
{
    if (!s_mx.enabled || (s_app_set && !s_app.power)) {
        led_driver_clear();
        return ESP_OK;
    }
    led_driver_set_brightness(s_app_set ? s_app.brightness : 128);
    switch (effect) {
        case RX_MATRIX_SPECTRUM_BARS: rx_spectrum_bars(feat); break;
        case RX_MATRIX_CENTER_VU:     rx_center_vu(feat);     break;
        case RX_MATRIX_BASS_PULSE:    rx_bass_pulse(feat);    break;
        case RX_MATRIX_AUDIO_RIPPLE:  rx_audio_ripple(feat);  break;
        case RX_MATRIX_BEAT_FLASH:    rx_beat_flash(feat);    break;
        case RX_MATRIX_SPARK_FIELD:   rx_spark_field(feat);   break;
        case RX_MATRIX_AUDIO_PLASMA:  rx_audio_plasma(feat);  break;
        case RX_MATRIX_FIRE_EQ:       rx_fire_eq(feat);       break;
        case RX_MATRIX_BASS_TUNNEL:   rx_bass_tunnel(feat);   break;
        case RX_MATRIX_TREBLE_RAIN:   rx_treble_rain(feat);   break;
        case RX_MATRIX_MID_WAVE:      rx_mid_wave(feat);      break;
        case RX_MATRIX_AUDIO_BLOCKS:  rx_audio_blocks(feat);  break;
        case RX_MATRIX_REACTIVE_AURORA: rx_reactive_aurora(feat); break;
        case RX_MATRIX_REACTIVE_SPIRAL: rx_reactive_spiral(feat); break;
        case RX_MATRIX_SPECTRUM_RAINBOW: rx_spectrum_rainbow(feat); break;
        case RX_MATRIX_SPECTRUM_MIRROR: rx_spectrum_mirror(feat); break;
        case RX_MATRIX_SPECTRUM_PEAKS: rx_spectrum_peaks(feat); break;
        case RX_MATRIX_SPECTRUM_WATERFALL: rx_spectrum_waterfall(feat); break;
        case RX_MATRIX_SEGMENT_VU:     rx_segment_vu(feat); break;
        default:                      rx_spectrum_bars(feat); break;
    }
    M.frame++;
    return ESP_OK;
}
