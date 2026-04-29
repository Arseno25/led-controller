#include "app_controller.h"
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "config_manager.h"
#include "led_driver.h"
#include "animation_layer.h"
#include "audio_input.h"
#include "audio_processor.h"
#include "reactive_engine.h"
#include "reactive_renderer.h"
#include "reactive_matrix_renderer.h"
#include "matrix_engine.h"
#include "random_effect_manager.h"
#include "palette_manager.h"
#include "mode_manager.h"
#include "system_monitor.h"
#include "wifi_service.h"
#include "web_server.h"
#include "DisplayManager.h"

static const char *TAG = "app_ctrl";
static app_config_t s_config = {0};
static bool s_initialized = false;
static TaskHandle_t s_render_task = NULL;
static TaskHandle_t s_save_task = NULL;
static TaskHandle_t s_display_task = NULL;
static SemaphoreHandle_t s_config_mux = NULL;
static app_config_t s_web_snapshot = {0};
static bool s_save_dirty = false;
static uint32_t s_save_due_ms = 0;
static uint32_t s_save_count = 0;
static esp_err_t s_last_save_err = ESP_OK;

#define RENDER_TASK_STACK 6144
#define RENDER_TASK_PRIO  4
#define RENDER_FPS_MS     33   /* ~30 FPS */
#define CONFIG_SAVE_DEBOUNCE_MS 1200
#define CONFIG_SAVE_RETRY_MS    2500
#define CONFIG_SAVE_TASK_STACK  4096
#define CONFIG_SAVE_TASK_PRIO   2
#define DISPLAY_TASK_STACK      6144
#define DISPLAY_TASK_PRIO       2
#define DISPLAY_TASK_INTERVAL_MS 33

static uint32_t app_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static bool time_reached(uint32_t now, uint32_t due)
{
    return (int32_t)(now - due) >= 0;
}

static void monitor_save_state(void)
{
    system_monitor_set_config_save_state(s_save_dirty, s_save_count, s_last_save_err);
}

static void config_lock(void)
{
    if (s_config_mux) xSemaphoreTake(s_config_mux, portMAX_DELAY);
}

static void config_unlock(void)
{
    if (s_config_mux) xSemaphoreGive(s_config_mux);
}

static void config_snapshot(app_config_t *out)
{
    if (!out) return;
    config_lock();
    *out = s_config;
    config_unlock();
}

static void config_mark_dirty_locked(uint32_t delay_ms)
{
    s_save_dirty = true;
    s_save_due_ms = app_now_ms() + delay_ms;
    monitor_save_state();
}

static void config_store(const app_config_t *cfg, bool persist)
{
    if (!cfg) return;
    config_lock();
    s_config = *cfg;
    if (persist) config_mark_dirty_locked(CONFIG_SAVE_DEBOUNCE_MS);
    config_unlock();
}

static void config_save_task(void *pv)
{
    (void)pv;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(250));

        app_config_t snapshot;
        bool should_save = false;
        uint32_t now = app_now_ms();

        config_lock();
        if (s_save_dirty && time_reached(now, s_save_due_ms)) {
            snapshot = s_config;
            s_save_dirty = false;
            should_save = true;
            monitor_save_state();
        }
        config_unlock();

        if (!should_save) continue;

        esp_err_t err = config_manager_save(&snapshot);

        config_lock();
        s_last_save_err = err;
        if (err == ESP_OK) {
            s_save_count++;
        } else {
            s_save_dirty = true;
            s_save_due_ms = app_now_ms() + CONFIG_SAVE_RETRY_MS;
            ESP_LOGW(TAG, "deferred config save failed: %s", esp_err_to_name(err));
        }
        monitor_save_state();
        config_unlock();
    }
}

static void display_task(void *pv)
{
    (void)pv;
    while (1) {
        app_config_t cfg;
        audio_features_t audio;
        system_monitor_snapshot_t system;

        config_snapshot(&cfg);
        if (audio_processor_get_features(&audio) != ESP_OK) {
            memset(&audio, 0, sizeof(audio));
        }
        system_monitor_get_snapshot(&system);
        display_manager_update(&cfg, &audio, &system);
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_TASK_INTERVAL_MS));
    }
}

static void config_to_driver(const app_config_t *cfg, led_driver_config_t *out)
{
    out->type       = cfg->led_type;
    out->pin        = cfg->led_pin;
    out->count      = cfg->led_count;
    out->brightness = cfg->brightness;
    out->rgbw       = led_driver_type_supports_white(cfg->led_type);
}

static void config_to_animation(const app_config_t *cfg, animation_config_t *out)
{
    out->type             = cfg->animation;
    out->primary_color    = (led_color_t){cfg->color_r, cfg->color_g, cfg->color_b, cfg->color_w};
    out->secondary_color  = (led_color_t){cfg->sec_r, cfg->sec_g, cfg->sec_b, cfg->sec_w};
    out->background_color = (led_color_t){cfg->bg_r, cfg->bg_g, cfg->bg_b, cfg->bg_w};
    out->speed            = cfg->animation_speed;
    out->brightness       = cfg->brightness;
    out->direction        = cfg->animation_direction;
    out->size             = cfg->animation_size;
    out->tail_length      = cfg->tail_length;
    out->fade_amount      = cfg->fade_amount;
    out->density          = cfg->density;
    out->intensity        = cfg->intensity;
    out->cooling          = cfg->cooling;
    out->sparking         = cfg->sparking;
    out->loop             = cfg->anim_loop;
    out->mirror           = cfg->mirror;
    out->random_color     = cfg->random_color;
    out->custom_pattern   = cfg->custom_pattern;
    out->power            = cfg->power;
}

static uint32_t render_interval_ms_for(const app_config_t *cfg)
{
    if (!cfg) return RENDER_FPS_MS;
    if (cfg->led_count > 720) return 50;  /* WS2812 refresh dominates at high counts. */
    if (cfg->led_count > 420) return 40;
    return RENDER_FPS_MS;
}

static void render_task(void *pv)
{
    (void)pv;
    uint32_t last_tick = app_now_ms();
    while (1) {
        uint32_t frame_start = app_now_ms();
        uint32_t now = frame_start;
        uint32_t delta_ms = now - last_tick;
        last_tick = now;

        app_config_t cfg;
        config_snapshot(&cfg);
        operating_mode_t m = mode_manager_get_mode();

        /* Random effect autoplay */
        if (m == OPERATING_MODE_REACTIVE || m == OPERATING_MODE_REACTIVE_MATRIX) {
            reactive_effect_t  rx = cfg.active_reactive_effect;
            rx_matrix_effect_t mx = cfg.active_rx_matrix_effect;
            if (random_effect_manager_tick_reactive(m, &rx, &mx, delta_ms)) {
                if (m == OPERATING_MODE_REACTIVE) {
                    cfg.active_reactive_effect = rx;
                    config_store(&cfg, true);
                    reactive_renderer_set_effect(rx);
                } else {
                    cfg.active_rx_matrix_effect = mx;
                    config_store(&cfg, true);
                    reactive_matrix_renderer_set_effect(mx);
                }
            }
        } else if (m == OPERATING_MODE_NORMAL) {
            animation_type_t a = cfg.animation;
            if (random_effect_manager_tick_normal(&a, delta_ms)) {
                cfg.animation = a;
                config_store(&cfg, true);
                animation_config_t ac;
                config_to_animation(&cfg, &ac);
                animation_layer_set_config(&ac);
            }
        }

        mode_manager_update(delta_ms);
        system_monitor_record_render(app_now_ms() - frame_start);
        vTaskDelay(pdMS_TO_TICKS(render_interval_ms_for(&cfg)));
    }
}

esp_err_t app_controller_apply_config(void)
{
    app_config_t cfg;
    config_snapshot(&cfg);

    animation_config_t a;
    config_to_animation(&cfg, &a);
    esp_err_t err = animation_layer_set_config(&a);

    reactive_renderer_set_params(&cfg);
    reactive_renderer_set_effect(cfg.active_reactive_effect);

    reactive_matrix_renderer_set_params(&cfg);
    reactive_matrix_renderer_set_effect(cfg.active_rx_matrix_effect);

    matrix_engine_set_config(&cfg.matrix);
    matrix_engine_set_params(&cfg);
    mode_manager_set_matrix_effect(cfg.active_matrix_effect);

    random_effect_manager_set_random_reactive(&cfg.random_reactive);
    random_effect_manager_set_random_normal(&cfg.random_normal);

    return err;
}

static esp_err_t reinit_driver(void)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    led_driver_config_t d;
    config_to_driver(&cfg, &d);
    animation_layer_pause();
    esp_err_t err = led_driver_reinit(&d);
    animation_layer_resume();
    return err;
}

/* ---- Web server callbacks ---- */

static esp_err_t cb_config_update(const app_config_t *new_cfg)
{
    if (!new_cfg) return ESP_ERR_INVALID_ARG;
    app_config_t old_cfg;
    config_snapshot(&old_cfg);
    bool reinit = (new_cfg->led_type  != old_cfg.led_type) ||
                  (new_cfg->led_pin   != old_cfg.led_pin)  ||
                  (new_cfg->led_count != old_cfg.led_count);
    config_store(new_cfg, true);
    if (reinit) {
        ESP_LOGI(TAG, "reinit driver");
        esp_err_t err = reinit_driver();
        if (err != ESP_OK) return err;
    }
    return app_controller_apply_config();
}

static esp_err_t cb_color_update(const led_color_t *color)
{
    if (!color) return ESP_ERR_INVALID_ARG;
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.color_r = color->r; cfg.color_g = color->g;
    cfg.color_b = color->b; cfg.color_w = color->w;
    config_store(&cfg, true);
    return app_controller_apply_config();
}

static esp_err_t cb_brightness_update(uint8_t brightness)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.brightness = brightness;
    config_store(&cfg, true);
    reactive_renderer_set_params(&cfg);
    reactive_matrix_renderer_set_params(&cfg);
    matrix_engine_set_params(&cfg);
    return animation_layer_set_brightness(brightness);
}

static esp_err_t cb_power_update(bool power)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.power = power;
    config_store(&cfg, true);
    reactive_renderer_set_params(&cfg);
    reactive_matrix_renderer_set_params(&cfg);
    matrix_engine_set_params(&cfg);
    return animation_layer_set_power(power);
}

static esp_err_t cb_animation_update(const app_config_t *anim_cfg)
{
    if (!anim_cfg) return ESP_ERR_INVALID_ARG;
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.animation           = anim_cfg->animation;
    cfg.animation_speed     = anim_cfg->animation_speed;
    cfg.animation_direction = anim_cfg->animation_direction;
    cfg.color_r = anim_cfg->color_r; cfg.color_g = anim_cfg->color_g;
    cfg.color_b = anim_cfg->color_b; cfg.color_w = anim_cfg->color_w;
    cfg.sec_r = anim_cfg->sec_r; cfg.sec_g = anim_cfg->sec_g;
    cfg.sec_b = anim_cfg->sec_b; cfg.sec_w = anim_cfg->sec_w;
    cfg.bg_r = anim_cfg->bg_r; cfg.bg_g = anim_cfg->bg_g;
    cfg.bg_b = anim_cfg->bg_b; cfg.bg_w = anim_cfg->bg_w;
    cfg.animation_size = anim_cfg->animation_size;
    cfg.tail_length    = anim_cfg->tail_length;
    cfg.fade_amount    = anim_cfg->fade_amount;
    cfg.density        = anim_cfg->density;
    cfg.intensity      = anim_cfg->intensity;
    cfg.cooling        = anim_cfg->cooling;
    cfg.sparking       = anim_cfg->sparking;
    cfg.anim_loop      = anim_cfg->anim_loop;
    cfg.mirror         = anim_cfg->mirror;
    cfg.random_color   = anim_cfg->random_color;
    cfg.custom_pattern = anim_cfg->custom_pattern;
    config_store(&cfg, true);
    return app_controller_apply_config();
}

static esp_err_t cb_mode_update(operating_mode_t mode)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.operating_mode = mode;
    if ((mode == OPERATING_MODE_MATRIX || mode == OPERATING_MODE_REACTIVE_MATRIX) &&
        !cfg.matrix.enabled) {
        cfg.matrix.enabled = true;
        matrix_engine_set_config(&cfg.matrix);
        matrix_engine_set_params(&cfg);
    }
    config_store(&cfg, true);

    esp_err_t err = mode_manager_set_mode(mode);
    if (err != ESP_OK) return err;

    /* Audio pipeline runs only when reactive */
    bool need_audio = (mode == OPERATING_MODE_REACTIVE || mode == OPERATING_MODE_REACTIVE_MATRIX);
    if (need_audio) err = reactive_engine_start(); else err = reactive_engine_stop();
    system_monitor_set_audio_running(reactive_engine_is_running());
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "mode -> %s", config_manager_operating_mode_to_string(mode));
    return ESP_OK;
}

static esp_err_t cb_reactive_effect_update(reactive_effect_t effect)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.active_reactive_effect = effect;
    config_store(&cfg, true);
    return reactive_renderer_set_effect(effect);
}

static esp_err_t cb_matrix_effect_update(matrix_effect_t effect)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.active_matrix_effect = effect;
    config_store(&cfg, true);
    return mode_manager_set_matrix_effect(effect);
}

static esp_err_t cb_rx_matrix_effect_update(rx_matrix_effect_t effect)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.active_rx_matrix_effect = effect;
    config_store(&cfg, true);
    return reactive_matrix_renderer_set_effect(effect);
}

static esp_err_t cb_audio_config_update(const audio_config_t *au)
{
    if (!au) return ESP_ERR_INVALID_ARG;
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.audio = *au;
    config_store(&cfg, true);
    esp_err_t err = reactive_engine_set_config(au);
    system_monitor_set_audio_running(reactive_engine_is_running());
    return err;
}

static esp_err_t cb_get_audio_features(audio_features_t *out)
{
    return audio_processor_get_features(out);
}

static esp_err_t cb_matrix_config_update(const matrix_config_t *mx)
{
    if (!mx) return ESP_ERR_INVALID_ARG;
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.matrix = *mx;
    config_store(&cfg, true);
    matrix_engine_set_config(mx);
    return ESP_OK;
}

static esp_err_t cb_random_reactive_update(const random_reactive_config_t *rr)
{
    if (!rr) return ESP_ERR_INVALID_ARG;
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.random_reactive = *rr;
    config_store(&cfg, true);
    return random_effect_manager_set_random_reactive(rr);
}

static esp_err_t cb_random_normal_update(const random_normal_config_t *rn)
{
    if (!rn) return ESP_ERR_INVALID_ARG;
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.random_normal = *rn;
    config_store(&cfg, true);
    return random_effect_manager_set_random_normal(rn);
}

static esp_err_t cb_random_next(operating_mode_t mode)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    if (mode == OPERATING_MODE_NORMAL) {
        animation_type_t cur = cfg.animation;
        animation_type_t pick = cur;
        int range = ANIM_TYPE_MAX - 1;
        for (int i = 0; i < 10; i++) {
            pick = (animation_type_t)(esp_random() % range);
            if (pick != cur || range <= 1) break;
        }
        cfg.animation = pick;
        config_store(&cfg, true);
        animation_config_t ac;
        config_to_animation(&cfg, &ac);
        return animation_layer_set_config(&ac);
    }
    if (mode == OPERATING_MODE_MATRIX) {
        matrix_effect_t cur = cfg.active_matrix_effect;
        matrix_effect_t pick = cur;
        for (int i = 0; i < 10; i++) {
            pick = (matrix_effect_t)(esp_random() % MATRIX_EFFECT_MAX);
            if (pick != cur || MATRIX_EFFECT_MAX <= 1) break;
        }
        cfg.active_matrix_effect = pick;
        config_store(&cfg, true);
        return mode_manager_set_matrix_effect(pick);
    }
    if (mode == OPERATING_MODE_REACTIVE) {
        reactive_effect_t cur = cfg.active_reactive_effect;
        reactive_effect_t pick = cur;
        for (int i = 0; i < 10; i++) {
            pick = (reactive_effect_t)(esp_random() % REACTIVE_EFFECT_MAX);
            if (pick != cur || REACTIVE_EFFECT_MAX <= 1) break;
        }
        cfg.active_reactive_effect = pick;
        config_store(&cfg, true);
        return reactive_renderer_set_effect(pick);
    }
    if (mode == OPERATING_MODE_REACTIVE_MATRIX) {
        rx_matrix_effect_t cur = cfg.active_rx_matrix_effect;
        rx_matrix_effect_t pick = cur;
        for (int i = 0; i < 10; i++) {
            pick = (rx_matrix_effect_t)(esp_random() % RX_MATRIX_MAX);
            if (pick != cur || RX_MATRIX_MAX <= 1) break;
        }
        cfg.active_rx_matrix_effect = pick;
        config_store(&cfg, true);
        return reactive_matrix_renderer_set_effect(pick);
    }
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t cb_palette_update(uint8_t pid)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.palette_id = pid;
    config_store(&cfg, true);
    reactive_renderer_set_params(&cfg);
    reactive_matrix_renderer_set_params(&cfg);
    matrix_engine_set_params(&cfg);
    return ESP_OK;
}

static esp_err_t cb_display_config_update(const display_config_t *display)
{
    if (!display) return ESP_ERR_INVALID_ARG;
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.display = *display;
    if (config_manager_validate(&cfg) != ESP_OK) return ESP_ERR_INVALID_ARG;
    config_store(&cfg, true);
    display_manager_apply_config(display);
    return ESP_OK;
}

static esp_err_t cb_display_view_update(display_view_mode_t view)
{
    app_config_t cfg;
    config_snapshot(&cfg);
    cfg.display.view_mode = view;
    if (config_manager_validate(&cfg) != ESP_OK) return ESP_ERR_INVALID_ARG;
    config_store(&cfg, true);
    display_manager_apply_config(&cfg.display);
    return ESP_OK;
}

static esp_err_t cb_factory_reset(void)
{
    reactive_engine_stop();
    system_monitor_set_audio_running(false);
    esp_err_t err = config_manager_factory_reset();
    if (err != ESP_OK) return err;
    app_config_t cfg;
    config_manager_default(&cfg);
    config_store(&cfg, false);
    config_lock();
    s_save_dirty = false;
    s_last_save_err = ESP_OK;
    monitor_save_state();
    config_unlock();
    err = reinit_driver();
    if (err != ESP_OK) return err;
    mode_manager_set_mode(OPERATING_MODE_NORMAL);
    reactive_renderer_reset();
    reactive_matrix_renderer_reset();
    matrix_engine_reset();
    return app_controller_apply_config();
}

static const app_config_t *cb_get_config(void)
{
    config_snapshot(&s_web_snapshot);
    return &s_web_snapshot;
}

esp_err_t app_controller_init(void)
{
    if (s_initialized) return ESP_OK;
    system_monitor_init();
    if (!s_config_mux) {
        s_config_mux = xSemaphoreCreateMutex();
        if (!s_config_mux) return ESP_ERR_NO_MEM;
    }
    esp_err_t err = config_manager_init();
    if (err != ESP_OK) return err;
    err = config_manager_load(&s_config);
    if (err != ESP_OK) return err;
    s_initialized = true;
    ESP_LOGI(TAG, "init done device=%s mode=%s",
             s_config.device_name,
             config_manager_operating_mode_to_string(s_config.operating_mode));
    return ESP_OK;
}

esp_err_t app_controller_start(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    app_config_t cfg;
    config_snapshot(&cfg);

    led_driver_config_t dcfg;
    config_to_driver(&cfg, &dcfg);
    esp_err_t err = led_driver_init(&dcfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "led_driver_init"); return err; }

    err = animation_layer_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "anim_init"); return err; }

    err = palette_manager_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "palette_init"); return err; }

    err = audio_input_init(&cfg.audio);
    if (err != ESP_OK) ESP_LOGW(TAG, "audio_input_init=%d", err);

    err = audio_processor_init(&cfg.audio);
    if (err != ESP_OK) ESP_LOGW(TAG, "audio_processor_init=%d", err);

    err = reactive_engine_init(&cfg.audio);
    if (err != ESP_OK) ESP_LOGW(TAG, "reactive_engine_init=%d", err);

    err = reactive_renderer_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "rx_render_init"); return err; }

    err = reactive_matrix_renderer_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "rx_mtx_render_init"); return err; }

    err = matrix_engine_init(&cfg.matrix);
    if (err != ESP_OK) { ESP_LOGE(TAG, "matrix_engine_init"); return err; }

    err = random_effect_manager_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "random_init=%d", err);

    err = mode_manager_init(cfg.operating_mode);
    if (err != ESP_OK) { ESP_LOGE(TAG, "mode_mgr_init"); return err; }

    err = app_controller_apply_config();
    if (err != ESP_OK) ESP_LOGW(TAG, "initial apply fail=%d", err);

    err = display_manager_begin(&cfg.display);
    if (err != ESP_OK) ESP_LOGW(TAG, "display_init=%d", err);

    if (cfg.operating_mode == OPERATING_MODE_REACTIVE ||
        cfg.operating_mode == OPERATING_MODE_REACTIVE_MATRIX) {
        reactive_engine_start();
    }
    system_monitor_set_audio_running(reactive_engine_is_running());

    if (!s_save_task &&
        xTaskCreate(config_save_task, "cfg_save", CONFIG_SAVE_TASK_STACK, NULL,
                    CONFIG_SAVE_TASK_PRIO, &s_save_task) != pdPASS) {
        ESP_LOGE(TAG, "config save task fail");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(render_task, "render", RENDER_TASK_STACK, NULL, RENDER_TASK_PRIO,
                    &s_render_task) != pdPASS) {
        ESP_LOGE(TAG, "render task fail");
        return ESP_ERR_NO_MEM;
    }

    if (!s_display_task &&
        xTaskCreate(display_task, "display", DISPLAY_TASK_STACK, NULL, DISPLAY_TASK_PRIO,
                    &s_display_task) != pdPASS) {
        ESP_LOGE(TAG, "display task fail");
        return ESP_ERR_NO_MEM;
    }

    err = wifi_service_start_ap();
    if (err != ESP_OK) { ESP_LOGE(TAG, "wifi"); return err; }

    web_server_callbacks_t cb = {
        .on_config_update          = cb_config_update,
        .on_color_update           = cb_color_update,
        .on_brightness_update      = cb_brightness_update,
        .on_power_update           = cb_power_update,
        .on_animation_update       = cb_animation_update,
        .on_factory_reset          = cb_factory_reset,
        .get_config                = cb_get_config,
        .on_audio_config_update    = cb_audio_config_update,
        .get_audio_features        = cb_get_audio_features,
        .on_mode_update            = cb_mode_update,
        .on_reactive_effect_update = cb_reactive_effect_update,
        .on_matrix_effect_update   = cb_matrix_effect_update,
        .on_rx_matrix_effect_update = cb_rx_matrix_effect_update,
        .on_matrix_config_update   = cb_matrix_config_update,
        .on_random_reactive_update = cb_random_reactive_update,
        .on_random_normal_update   = cb_random_normal_update,
        .on_random_next            = cb_random_next,
        .on_palette_update         = cb_palette_update,
        .on_display_config_update  = cb_display_config_update,
        .on_display_view_update    = cb_display_view_update,
    };
    err = web_server_start(&cb);
    if (err != ESP_OK) { ESP_LOGE(TAG, "web"); return err; }

    ESP_LOGI(TAG, "ready mode=%s", config_manager_operating_mode_to_string(cfg.operating_mode));
    return ESP_OK;
}

esp_err_t app_controller_factory_reset(void) { return cb_factory_reset(); }
