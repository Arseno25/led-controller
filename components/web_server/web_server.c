#include "web_server.h"
#include "cJSON.h"
#include "effect_registry.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "palette_manager.h"
#include "system_monitor.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "web_server";
#define MAX_BODY_SIZE 2048

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server = NULL;
static web_server_callbacks_t s_cb = {0};

static esp_err_t send_json(httpd_req_t *req, cJSON *root, int status) {

  char *out = cJSON_PrintUnformatted(root);
  if (!out)
    return httpd_resp_send_500(req);
  httpd_resp_set_type(req, "application/json");
  if (status == 400)
    httpd_resp_set_status(req, "400 Bad Request");
  else if (status == 500)
    httpd_resp_set_status(req, "500 Internal Server Error");
  esp_err_t err = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
  free(out);
  return err;
}
static esp_err_t send_error(httpd_req_t *req, int s, const char *m) {

  cJSON *r = cJSON_CreateObject();
  cJSON_AddBoolToObject(r, "success", false);
  cJSON_AddStringToObject(r, "message", m);
  esp_err_t e = send_json(req, r, s);
  cJSON_Delete(r);
  return e;
}
static esp_err_t send_success(httpd_req_t *req, const char *m) {

  cJSON *r = cJSON_CreateObject();
  cJSON_AddBoolToObject(r, "success", true);
  if (m)
    cJSON_AddStringToObject(r, "message", m);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}
static esp_err_t read_json_body(httpd_req_t *req, cJSON **out) {

  if (req->content_len == 0 || req->content_len > MAX_BODY_SIZE)
    return ESP_ERR_INVALID_SIZE;
  char *buf = malloc(req->content_len + 1);
  if (!buf)
    return ESP_ERR_NO_MEM;
  int total = 0;
  while (total < req->content_len) {
    int got = httpd_req_recv(req, buf + total, req->content_len - total);
    if (got <= 0) {
      free(buf);
      return ESP_FAIL;
    }
    total += got;
  }
  buf[total] = '\0';
  cJSON *json = cJSON_Parse(buf);
  free(buf);
  if (!json)
    return ESP_ERR_INVALID_ARG;
  *out = json;
  return ESP_OK;
}
static bool get_u8(const cJSON *r, const char *k, uint8_t *o) {

  const cJSON *v = cJSON_GetObjectItemCaseSensitive(r, k);
  if (!cJSON_IsNumber(v) || v->valueint < 0 || v->valueint > 255)
    return false;
  *o = (uint8_t)v->valueint;
  return true;
}
static bool get_bool_obj(const cJSON *r, const char *k, bool *o) {

  const cJSON *v = cJSON_GetObjectItemCaseSensitive(r, k);
  if (!cJSON_IsBool(v))
    return false;
  *o = cJSON_IsTrue(v);
  return true;
}
static bool get_u16_obj(const cJSON *r, const char *k, uint16_t *o) {

  const cJSON *v = cJSON_GetObjectItemCaseSensitive(r, k);
  if (!cJSON_IsNumber(v) || v->valueint < 0 || v->valueint > 65535)
    return false;
  *o = (uint16_t)v->valueint;
  return true;
}
static bool get_query_param(httpd_req_t *req, const char *key, char *out,
                            size_t out_len) {

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
    return false;
  return httpd_query_key_value(query, key, out, out_len) == ESP_OK;
}
static bool parse_color(const cJSON *c, led_color_t *o) {

  if (!cJSON_IsObject(c))
    return false;
  return get_u8(c, "r", &o->r) && get_u8(c, "g", &o->g) &&
         get_u8(c, "b", &o->b) && get_u8(c, "w", &o->w);
}
static void add_color_obj(cJSON *p, const char *n, uint8_t r, uint8_t g,
                          uint8_t b, uint8_t w) {

  cJSON *c = cJSON_AddObjectToObject(p, n);
  cJSON_AddNumberToObject(c, "r", r);
  cJSON_AddNumberToObject(c, "g", g);
  cJSON_AddNumberToObject(c, "b", b);
  cJSON_AddNumberToObject(c, "w", w);
}

static cJSON *config_to_json(const app_config_t *cfg) {

  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "device_name", cfg->device_name);
  cJSON *bj = cJSON_AddObjectToObject(r, "board");
  cJSON_AddStringToObject(bj, "target", BOARD_TARGET_NAME);
  cJSON_AddNumberToObject(bj, "default_led_pin", BOARD_DEFAULT_LED_PIN);
  cJSON_AddNumberToObject(bj, "default_i2s_bclk_pin", BOARD_DEFAULT_I2S_BCLK_PIN);
  cJSON_AddNumberToObject(bj, "default_i2s_ws_pin", BOARD_DEFAULT_I2S_WS_PIN);
  cJSON_AddNumberToObject(bj, "default_i2s_data_pin", BOARD_DEFAULT_I2S_DATA_PIN);
  cJSON_AddNumberToObject(bj, "default_tft_sclk_pin", BOARD_DEFAULT_TFT_SCLK_PIN);
  cJSON_AddNumberToObject(bj, "default_tft_mosi_pin", BOARD_DEFAULT_TFT_MOSI_PIN);
  cJSON_AddNumberToObject(bj, "default_tft_cs_pin", BOARD_DEFAULT_TFT_CS_PIN);
  cJSON_AddNumberToObject(bj, "default_tft_dc_pin", BOARD_DEFAULT_TFT_DC_PIN);
  cJSON_AddNumberToObject(bj, "default_tft_rst_pin", BOARD_DEFAULT_TFT_RST_PIN);
  cJSON_AddStringToObject(
      r, "operating_mode",
      config_manager_operating_mode_to_string(cfg->operating_mode));
  cJSON_AddStringToObject(r, "led_type",
                          config_manager_led_type_to_string(cfg->led_type));
  cJSON_AddNumberToObject(r, "led_pin", cfg->led_pin);
  cJSON_AddNumberToObject(r, "led_count", cfg->led_count);
  cJSON_AddNumberToObject(r, "brightness", cfg->brightness);
  cJSON_AddBoolToObject(r, "power", cfg->power);
  add_color_obj(r, "color", cfg->color_r, cfg->color_g, cfg->color_b,
                cfg->color_w);
  add_color_obj(r, "secondary_color", cfg->sec_r, cfg->sec_g, cfg->sec_b,
                cfg->sec_w);
  add_color_obj(r, "background_color", cfg->bg_r, cfg->bg_g, cfg->bg_b,
                cfg->bg_w);
  cJSON_AddStringToObject(r, "animation",
                          config_manager_animation_to_string(cfg->animation));
  cJSON_AddNumberToObject(r, "animation_speed", cfg->animation_speed);
  cJSON_AddStringToObject(
      r, "direction",
      config_manager_direction_to_string(cfg->animation_direction));
  cJSON_AddNumberToObject(r, "size", cfg->animation_size);
  cJSON_AddNumberToObject(r, "tail_length", cfg->tail_length);
  cJSON_AddNumberToObject(r, "fade_amount", cfg->fade_amount);
  cJSON_AddNumberToObject(r, "density", cfg->density);
  cJSON_AddNumberToObject(r, "intensity", cfg->intensity);
  cJSON_AddNumberToObject(r, "cooling", cfg->cooling);
  cJSON_AddNumberToObject(r, "sparking", cfg->sparking);
  cJSON_AddBoolToObject(r, "loop", cfg->anim_loop);
  cJSON_AddBoolToObject(r, "mirror", cfg->mirror);
  cJSON_AddBoolToObject(r, "random_color", cfg->random_color);
  cJSON_AddStringToObject(
      r, "custom_pattern",
      config_manager_custom_pattern_to_string(cfg->custom_pattern));
  cJSON_AddStringToObject(
      r, "active_reactive_effect",
      config_manager_reactive_effect_to_string(cfg->active_reactive_effect));
  cJSON_AddStringToObject(
      r, "active_matrix_effect",
      config_manager_matrix_effect_to_string(cfg->active_matrix_effect));
  cJSON_AddStringToObject(
      r, "active_rx_matrix_effect",
      config_manager_rx_matrix_effect_to_string(cfg->active_rx_matrix_effect));
  cJSON_AddNumberToObject(r, "palette_id", cfg->palette_id);

  const audio_config_t *au = &cfg->audio;
  cJSON *aj = cJSON_AddObjectToObject(r, "audio");
  cJSON_AddNumberToObject(aj, "i2s_bclk_pin", au->i2s_bclk_pin);
  cJSON_AddNumberToObject(aj, "i2s_ws_pin",   au->i2s_ws_pin);
  cJSON_AddNumberToObject(aj, "i2s_data_pin", au->i2s_data_pin);
  cJSON_AddNumberToObject(aj, "sample_rate",  au->sample_rate);
  cJSON_AddNumberToObject(aj, "buffer_size",  au->buffer_size);
  cJSON_AddNumberToObject(aj, "fft_size",     au->fft_size);
  cJSON_AddNumberToObject(aj, "sensitivity",  au->sensitivity);
  cJSON_AddNumberToObject(aj, "gain",         au->gain);
  cJSON_AddBoolToObject(aj,   "auto_gain",    au->auto_gain);
  cJSON_AddNumberToObject(aj, "noise_gate",   au->noise_gate);
  cJSON_AddNumberToObject(aj, "smoothing",    au->smoothing);
  cJSON_AddNumberToObject(aj, "beat_threshold", au->beat_threshold);
  cJSON_AddBoolToObject(aj,   "fft_enabled",  au->fft_enabled);
  cJSON_AddNumberToObject(aj, "spectrum_bands", au->spectrum_bands);
  cJSON_AddBoolToObject(aj,   "bass_boost",   au->bass_boost);
  cJSON_AddBoolToObject(aj,   "treble_boost", au->treble_boost);

  const matrix_config_t *mx = &cfg->matrix;
  cJSON *mj = cJSON_AddObjectToObject(r, "matrix");
  cJSON_AddBoolToObject(mj,   "enabled",   mx->enabled);
  cJSON_AddNumberToObject(mj, "width",     mx->width);
  cJSON_AddNumberToObject(mj, "height",    mx->height);
  cJSON_AddStringToObject(mj, "layout",    config_manager_matrix_layout_to_string(mx->layout));
  cJSON_AddStringToObject(mj, "origin",    config_manager_matrix_origin_to_string(mx->origin));
  cJSON_AddBoolToObject(mj,   "reverse_x", mx->reverse_x);
  cJSON_AddBoolToObject(mj,   "reverse_y", mx->reverse_y);
  cJSON_AddBoolToObject(mj,   "rotate_90", mx->rotate_90);

  const random_reactive_config_t *rr = &cfg->random_reactive;
  cJSON *rrj = cJSON_AddObjectToObject(r, "random_reactive");
  cJSON_AddBoolToObject(rrj,   "enabled",          rr->enabled);
  cJSON_AddNumberToObject(rrj, "interval_seconds", rr->interval_seconds);
  cJSON_AddBoolToObject(rrj,   "no_repeat",        rr->no_repeat);
  cJSON_AddBoolToObject(rrj,   "include_strip",    rr->include_strip);
  cJSON_AddBoolToObject(rrj,   "include_matrix",   rr->include_matrix);
  cJSON_AddBoolToObject(rrj,   "only_favorites",   rr->only_favorites);

  const random_normal_config_t *rn = &cfg->random_normal;
  cJSON *rnj = cJSON_AddObjectToObject(r, "random_normal");
  cJSON_AddBoolToObject(rnj,   "enabled",          rn->enabled);
  cJSON_AddNumberToObject(rnj, "interval_seconds", rn->interval_seconds);
  cJSON_AddBoolToObject(rnj,   "no_repeat",        rn->no_repeat);
  cJSON_AddBoolToObject(rnj,   "only_favorites",   rn->only_favorites);

  const display_config_t *dp = &cfg->display;
  cJSON *dj = cJSON_AddObjectToObject(r, "display");
  cJSON_AddBoolToObject(dj,   "enabled",    dp->enabled);
  cJSON_AddNumberToObject(dj, "brightness", dp->brightness);
  cJSON_AddStringToObject(dj, "theme",      config_manager_display_theme_to_string(dp->theme));
  cJSON_AddStringToObject(dj, "viewMode",   config_manager_display_view_to_string(dp->view_mode));
  cJSON_AddBoolToObject(dj,   "showFps",    dp->show_fps);
  cJSON_AddBoolToObject(dj,   "showWifi",   dp->show_wifi);

  return r;
}

static void parse_anim_fields(const cJSON *json, app_config_t *cfg) {

  const cJSON *v;
  uint8_t u8;
  v = cJSON_GetObjectItemCaseSensitive(json, "animation");
  if (cJSON_IsString(v) && v->valuestring) {
    animation_type_t a;
    if (config_manager_animation_from_string(v->valuestring, &a) == ESP_OK)
      cfg->animation = a;
  }
  if (get_u8(json, "speed", &u8))
    cfg->animation_speed = u8;
  if (get_u8(json, "animation_speed", &u8))
    cfg->animation_speed = u8;
  v = cJSON_GetObjectItemCaseSensitive(json, "direction");
  if (cJSON_IsString(v) && v->valuestring) {
    animation_direction_t d;
    if (config_manager_direction_from_string(v->valuestring, &d) == ESP_OK)
      cfg->animation_direction = d;
  }
  v = cJSON_GetObjectItemCaseSensitive(json, "color");
  if (cJSON_IsObject(v)) {
    led_color_t c;
    if (parse_color(v, &c)) {
      cfg->color_r = c.r;
      cfg->color_g = c.g;
      cfg->color_b = c.b;
      cfg->color_w = c.w;
    }
  }
  v = cJSON_GetObjectItemCaseSensitive(json, "secondary_color");
  if (cJSON_IsObject(v)) {
    led_color_t c;
    if (parse_color(v, &c)) {
      cfg->sec_r = c.r;
      cfg->sec_g = c.g;
      cfg->sec_b = c.b;
      cfg->sec_w = c.w;
    }
  }
  v = cJSON_GetObjectItemCaseSensitive(json, "background_color");
  if (cJSON_IsObject(v)) {
    led_color_t c;
    if (parse_color(v, &c)) {
      cfg->bg_r = c.r;
      cfg->bg_g = c.g;
      cfg->bg_b = c.b;
      cfg->bg_w = c.w;
    }
  }
  if (get_u8(json, "size", &u8))
    cfg->animation_size = u8;
  if (get_u8(json, "tail_length", &u8))
    cfg->tail_length = u8;
  if (get_u8(json, "fade_amount", &u8))
    cfg->fade_amount = u8;
  if (get_u8(json, "density", &u8))
    cfg->density = u8;
  if (get_u8(json, "intensity", &u8))
    cfg->intensity = u8;
  if (get_u8(json, "cooling", &u8))
    cfg->cooling = u8;
  if (get_u8(json, "sparking", &u8))
    cfg->sparking = u8;
  v = cJSON_GetObjectItemCaseSensitive(json, "loop");
  if (cJSON_IsBool(v))
    cfg->anim_loop = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(json, "mirror");
  if (cJSON_IsBool(v))
    cfg->mirror = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(json, "random_color");
  if (cJSON_IsBool(v))
    cfg->random_color = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(json, "custom_pattern");
  if (cJSON_IsString(v) && v->valuestring) {
    custom_pattern_type_t p;
    if (config_manager_custom_pattern_from_string(v->valuestring, &p) == ESP_OK)
      cfg->custom_pattern = p;
  }
}

/* --- HANDLERS --- */
static esp_err_t handle_root(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, index_html_start,
                         index_html_end - index_html_start);
}
static esp_err_t handle_get_config(httpd_req_t *req) {
  const app_config_t *c = s_cb.get_config();
  if (!c)
    return send_error(req, 500, "n/a");
  cJSON *r = config_to_json(c);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_post_config(httpd_req_t *req) {

  cJSON *json = NULL;
  if (read_json_body(req, &json) != ESP_OK)
    return send_error(req, 400, "invalid json");
  const app_config_t *cur = s_cb.get_config();
  app_config_t nc = *cur;
  const cJSON *v;
  uint8_t u8;
  v = cJSON_GetObjectItemCaseSensitive(json, "device_name");
  if (cJSON_IsString(v) && v->valuestring) {
    strncpy(nc.device_name, v->valuestring, sizeof(nc.device_name) - 1);
  }
  v = cJSON_GetObjectItemCaseSensitive(json, "led_type");
  if (cJSON_IsString(v) && v->valuestring) {
    led_type_t t;
    if (config_manager_led_type_from_string(v->valuestring, &t) != ESP_OK) {
      cJSON_Delete(json);
      return send_error(req, 400, "bad led_type");
    }
    nc.led_type = t;
  }
  if (get_u8(json, "led_pin", &u8))
    nc.led_pin = u8;
  v = cJSON_GetObjectItemCaseSensitive(json, "led_count");
  if (cJSON_IsNumber(v))
    nc.led_count = (uint16_t)v->valueint;
  if (get_u8(json, "brightness", &u8))
    nc.brightness = u8;
  v = cJSON_GetObjectItemCaseSensitive(json, "power");
  if (cJSON_IsBool(v))
    nc.power = cJSON_IsTrue(v);
  parse_anim_fields(json, &nc);
  cJSON_Delete(json);
  if (config_manager_validate(&nc) != ESP_OK)
    return send_error(req, 400, "validation failed");
  if (s_cb.on_config_update(&nc) != ESP_OK)
    return send_error(req, 500, "apply failed");
  return send_success(req, "Config saved");
}

static esp_err_t handle_post_color(httpd_req_t *req) {
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK)
    return send_error(req, 400, "bad json");
  led_color_t c;
  if (!parse_color(j, &c)) {
    cJSON_Delete(j);
    return send_error(req, 400, "bad color");
  }
  cJSON_Delete(j);
  if (s_cb.on_color_update(&c) != ESP_OK)
    return send_error(req, 500, "fail");
  return send_success(req, "Color updated");
}
static esp_err_t handle_post_brightness(httpd_req_t *req) {
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK)
    return send_error(req, 400, "bad json");
  uint8_t b;
  if (!get_u8(j, "brightness", &b)) {
    cJSON_Delete(j);
    return send_error(req, 400, "bad val");
  }
  cJSON_Delete(j);
  if (s_cb.on_brightness_update(b) != ESP_OK)
    return send_error(req, 500, "fail");
  return send_success(req, "Brightness updated");
}
static esp_err_t handle_post_power(httpd_req_t *req) {
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK)
    return send_error(req, 400, "bad json");
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "power");
  if (!cJSON_IsBool(v)) {
    cJSON_Delete(j);
    return send_error(req, 400, "missing power");
  }
  bool pw = cJSON_IsTrue(v);
  cJSON_Delete(j);
  if (s_cb.on_power_update(pw) != ESP_OK)
    return send_error(req, 500, "fail");
  return send_success(req, pw ? "ON" : "OFF");
}
static esp_err_t handle_factory_reset(httpd_req_t *req) {
  if (s_cb.on_factory_reset() != ESP_OK)
    return send_error(req, 500, "fail");
  return send_success(req, "Factory reset done");
}

static esp_err_t handle_get_animation(httpd_req_t *req) {

  const app_config_t *cfg = s_cb.get_config();
  if (!cfg)
    return send_error(req, 500, "n/a");
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "animation",
                          config_manager_animation_to_string(cfg->animation));
  cJSON_AddNumberToObject(r, "speed", cfg->animation_speed);
  cJSON_AddStringToObject(
      r, "direction",
      config_manager_direction_to_string(cfg->animation_direction));
  add_color_obj(r, "color", cfg->color_r, cfg->color_g, cfg->color_b,
                cfg->color_w);
  add_color_obj(r, "secondary_color", cfg->sec_r, cfg->sec_g, cfg->sec_b,
                cfg->sec_w);
  add_color_obj(r, "background_color", cfg->bg_r, cfg->bg_g, cfg->bg_b,
                cfg->bg_w);
  cJSON_AddNumberToObject(r, "size", cfg->animation_size);
  cJSON_AddNumberToObject(r, "tail_length", cfg->tail_length);
  cJSON_AddNumberToObject(r, "fade_amount", cfg->fade_amount);
  cJSON_AddNumberToObject(r, "density", cfg->density);
  cJSON_AddNumberToObject(r, "intensity", cfg->intensity);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}
static esp_err_t handle_post_animation(httpd_req_t *req) {
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK)
    return send_error(req, 400, "bad json");
  const app_config_t *cur = s_cb.get_config();
  app_config_t nc = *cur;
  parse_anim_fields(j, &nc);
  cJSON_Delete(j);
  if (s_cb.on_animation_update(&nc) != ESP_OK)
    return send_error(req, 500, "fail");
  return send_success(req, "Animation applied");
}
static esp_err_t handle_get_animations(httpd_req_t *req) {

  cJSON *r = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(r, "animations");
  for (int i = 0; i < ANIM_TYPE_MAX; i++)
    cJSON_AddItemToArray(
        arr, cJSON_CreateString(
                 config_manager_animation_to_string((animation_type_t)i)));
  cJSON *dirs = cJSON_AddArrayToObject(r, "directions");
  for (int i = 0; i < ANIM_DIR_MAX; i++)
    cJSON_AddItemToArray(dirs,
                         cJSON_CreateString(config_manager_direction_to_string(
                             (animation_direction_t)i)));
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

/* --- MODE API --- */
static esp_err_t handle_get_mode(httpd_req_t *req) {

  const app_config_t *cfg = s_cb.get_config();
  if (!cfg)
    return send_error(req, 500, "n/a");
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(
      r, "operating_mode",
      config_manager_operating_mode_to_string(cfg->operating_mode));
  cJSON_AddStringToObject(r, "active_animation",
                          config_manager_animation_to_string(cfg->animation));
  cJSON_AddStringToObject(
      r, "active_reactive_effect",
      config_manager_reactive_effect_to_string(cfg->active_reactive_effect));
  cJSON_AddStringToObject(
      r, "active_matrix_effect",
      config_manager_matrix_effect_to_string(cfg->active_matrix_effect));
  cJSON_AddStringToObject(
      r, "active_rx_matrix_effect",
      config_manager_rx_matrix_effect_to_string(cfg->active_rx_matrix_effect));
  cJSON_AddBoolToObject(r, "matrix_enabled", cfg->matrix.enabled);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}
static esp_err_t handle_post_mode(httpd_req_t *req) {

  if (!s_cb.on_mode_update)
    return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK)
    return send_error(req, 400, "bad json");
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "operating_mode");
  if (!cJSON_IsString(v) || !v->valuestring) {
    cJSON_Delete(j);
    return send_error(req, 400, "missing operating_mode");
  }
  operating_mode_t m;
  if (config_manager_operating_mode_from_string(v->valuestring, &m) != ESP_OK) {
    cJSON_Delete(j);
    return send_error(req, 400, "invalid mode");
  }
  cJSON_Delete(j);
  if (s_cb.on_mode_update(m) != ESP_OK)
    return send_error(req, 500, "apply failed");
  cJSON *r = cJSON_CreateObject();
  cJSON_AddBoolToObject(r, "success", true);
  cJSON_AddStringToObject(r, "operating_mode",
                          config_manager_operating_mode_to_string(m));
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

/* --- REACTIVE EFFECTS API --- */
static esp_err_t handle_get_reactive_effects(httpd_req_t *req) {

  cJSON *r = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(r, "effects");
  for (int i = 0; i < REACTIVE_EFFECT_MAX; i++)
    cJSON_AddItemToArray(
        arr, cJSON_CreateString(config_manager_reactive_effect_to_string(
                 (reactive_effect_t)i)));
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static bool effect_supports_color(const effect_registry_item_t *item) {
  if (!item) return true;
  if (item->category == EFFECT_CATEGORY_REACTIVE) {
    return strcmp(item->name, "reactive_spectrum_bars") != 0;
  }
  if (item->category == EFFECT_CATEGORY_REACTIVE_MATRIX) {
    return strcmp(item->name, "matrix_center_vu") == 0 ||
           strcmp(item->name, "matrix_bass_pulse") == 0 ||
           strcmp(item->name, "matrix_audio_ripple") == 0 ||
           strcmp(item->name, "matrix_beat_flash") == 0 ||
           strcmp(item->name, "matrix_spark_field") == 0 ||
           strcmp(item->name, "matrix_mid_wave") == 0 ||
           strcmp(item->name, "matrix_segment_vu") == 0;
  }
  return true;
}

static void add_effect_item(cJSON *arr, const effect_registry_item_t *item) {

  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "id", item->id);
  cJSON_AddStringToObject(o, "name", item->name);
  cJSON_AddStringToObject(o, "label", item->label);
  cJSON_AddStringToObject(o, "category",
                          effect_registry_category_to_string(item->category));
  cJSON_AddBoolToObject(o, "requires_matrix", item->requires_matrix);
  cJSON_AddBoolToObject(o, "requires_audio", item->requires_audio);
  cJSON_AddBoolToObject(o, "supports_palette", item->supports_palette);
  cJSON_AddBoolToObject(o, "supports_color", effect_supports_color(item));
  cJSON_AddBoolToObject(o, "supports_random", item->supports_random);
  cJSON_AddItemToArray(arr, o);
}

static esp_err_t handle_get_effects(httpd_req_t *req) {

  char cat_str[32] = {0};
  bool has_cat = get_query_param(req, "category", cat_str, sizeof(cat_str));
  effect_category_t cat = EFFECT_CATEGORY_NORMAL;
  if (has_cat && !effect_registry_category_from_string(cat_str, &cat)) {
    return send_error(req, 400, "invalid category");
  }

  size_t count = 0;
  const effect_registry_item_t *items = effect_registry_get_all(&count);
  cJSON *r = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(r, "effects");
  for (size_t i = 0; i < count; i++) {
    if (!has_cat || items[i].category == cat) add_effect_item(arr, &items[i]);
  }
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_post_reactive_effect(httpd_req_t *req) {

  if (!s_cb.on_reactive_effect_update)
    return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK)
    return send_error(req, 400, "bad json");
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "effect");
  if (!cJSON_IsString(v) || !v->valuestring) {
    cJSON_Delete(j);
    return send_error(req, 400, "missing effect");
  }
  reactive_effect_t ef;
  if (config_manager_reactive_effect_from_string(v->valuestring, &ef) !=
      ESP_OK) {
    cJSON_Delete(j);
    return send_error(req, 400, "invalid effect");
  }
  cJSON_Delete(j);
  if (s_cb.on_reactive_effect_update(ef) != ESP_OK)
    return send_error(req, 500, "apply failed");
  cJSON *r = cJSON_CreateObject();
  cJSON_AddBoolToObject(r, "success", true);
  cJSON_AddStringToObject(r, "effect",
                          config_manager_reactive_effect_to_string(ef));
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_post_effect(httpd_req_t *req) {

  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK)
    return send_error(req, 400, "bad json");

  const cJSON *effect = cJSON_GetObjectItemCaseSensitive(j, "effect");
  const cJSON *category = cJSON_GetObjectItemCaseSensitive(j, "category");
  if (!cJSON_IsString(effect) || !effect->valuestring) {
    cJSON_Delete(j);
    return send_error(req, 400, "missing effect");
  }

  effect_category_t cat = EFFECT_CATEGORY_NORMAL;
  if (cJSON_IsString(category) && category->valuestring) {
    if (!effect_registry_category_from_string(category->valuestring, &cat)) {
      cJSON_Delete(j);
      return send_error(req, 400, "invalid category");
    }
  } else {
    const effect_registry_item_t *item =
        effect_registry_get_by_name(effect->valuestring);
    if (!item) {
      cJSON_Delete(j);
      return send_error(req, 400, "invalid effect");
    }
    cat = item->category;
  }

  esp_err_t err = ESP_ERR_NOT_SUPPORTED;
  if (cat == EFFECT_CATEGORY_NORMAL) {
    animation_type_t a;
    err = config_manager_animation_from_string(effect->valuestring, &a);
    if (err == ESP_OK && s_cb.get_config && s_cb.on_animation_update) {
      app_config_t nc = *s_cb.get_config();
      nc.animation = a;
      err = s_cb.on_animation_update(&nc);
    }
  } else if (cat == EFFECT_CATEGORY_REACTIVE && s_cb.on_reactive_effect_update) {
    reactive_effect_t ef;
    err = config_manager_reactive_effect_from_string(effect->valuestring, &ef);
    if (err == ESP_OK) err = s_cb.on_reactive_effect_update(ef);
  } else if (cat == EFFECT_CATEGORY_MATRIX && s_cb.on_matrix_effect_update) {
    matrix_effect_t ef;
    err = config_manager_matrix_effect_from_string(effect->valuestring, &ef);
    if (err == ESP_OK) err = s_cb.on_matrix_effect_update(ef);
  } else if (cat == EFFECT_CATEGORY_REACTIVE_MATRIX &&
             s_cb.on_rx_matrix_effect_update) {
    rx_matrix_effect_t ef;
    err = config_manager_rx_matrix_effect_from_string(effect->valuestring, &ef);
    if (err == ESP_OK) err = s_cb.on_rx_matrix_effect_update(ef);
  }
  cJSON_Delete(j);

  if (err != ESP_OK) return send_error(req, 400, "apply failed");
  return send_success(req, "Effect applied");
}

/* --- AUDIO (INMP441) CONFIG & STATE --- */
static esp_err_t handle_get_audio_config(httpd_req_t *req) {
  const app_config_t *cfg = s_cb.get_config();
  if (!cfg) return send_error(req, 500, "n/a");
  const audio_config_t *au = &cfg->audio;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddNumberToObject(r, "i2s_bclk_pin", au->i2s_bclk_pin);
  cJSON_AddNumberToObject(r, "i2s_ws_pin",   au->i2s_ws_pin);
  cJSON_AddNumberToObject(r, "i2s_data_pin", au->i2s_data_pin);
  cJSON_AddNumberToObject(r, "sample_rate",  au->sample_rate);
  cJSON_AddNumberToObject(r, "buffer_size",  au->buffer_size);
  cJSON_AddNumberToObject(r, "fft_size",     au->fft_size);
  cJSON_AddNumberToObject(r, "sensitivity",  au->sensitivity);
  cJSON_AddNumberToObject(r, "gain",         au->gain);
  cJSON_AddBoolToObject(r,   "auto_gain",    au->auto_gain);
  cJSON_AddNumberToObject(r, "noise_gate",   au->noise_gate);
  cJSON_AddNumberToObject(r, "smoothing",    au->smoothing);
  cJSON_AddNumberToObject(r, "beat_threshold", au->beat_threshold);
  cJSON_AddBoolToObject(r,   "fft_enabled",  au->fft_enabled);
  cJSON_AddNumberToObject(r, "spectrum_bands", au->spectrum_bands);
  cJSON_AddBoolToObject(r,   "bass_boost",   au->bass_boost);
  cJSON_AddBoolToObject(r,   "treble_boost", au->treble_boost);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_post_audio_config(httpd_req_t *req) {
  if (!s_cb.on_audio_config_update) return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK) return send_error(req, 400, "bad json");
  const app_config_t *cur = s_cb.get_config();
  audio_config_t au = cur->audio;
  const cJSON *v;
  uint8_t u8;
  v = cJSON_GetObjectItemCaseSensitive(j, "i2s_bclk_pin"); if (cJSON_IsNumber(v)) au.i2s_bclk_pin = (int8_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "i2s_ws_pin");   if (cJSON_IsNumber(v)) au.i2s_ws_pin   = (int8_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "i2s_data_pin"); if (cJSON_IsNumber(v)) au.i2s_data_pin = (int8_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "sample_rate");  if (cJSON_IsNumber(v)) au.sample_rate  = (uint32_t)v->valuedouble;
  v = cJSON_GetObjectItemCaseSensitive(j, "buffer_size");  if (cJSON_IsNumber(v)) au.buffer_size  = (uint16_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "fft_size");     if (cJSON_IsNumber(v)) au.fft_size     = (uint16_t)v->valueint;
  if (get_u8(j, "sensitivity", &u8)) au.sensitivity = u8;
  if (get_u8(j, "gain", &u8))        au.gain = u8;
  v = cJSON_GetObjectItemCaseSensitive(j, "auto_gain");    if (cJSON_IsBool(v))   au.auto_gain = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "noise_gate");   if (cJSON_IsNumber(v)) au.noise_gate = (uint16_t)v->valueint;
  if (get_u8(j, "smoothing", &u8))   au.smoothing = u8;
  v = cJSON_GetObjectItemCaseSensitive(j, "beat_threshold"); if (cJSON_IsNumber(v)) au.beat_threshold = (uint16_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "fft_enabled");  if (cJSON_IsBool(v))   au.fft_enabled = cJSON_IsTrue(v);
  if (get_u8(j, "spectrum_bands", &u8)) au.spectrum_bands = u8;
  v = cJSON_GetObjectItemCaseSensitive(j, "bass_boost");   if (cJSON_IsBool(v))   au.bass_boost = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "treble_boost"); if (cJSON_IsBool(v))   au.treble_boost = cJSON_IsTrue(v);
  cJSON_Delete(j);
  app_config_t next = *cur;
  next.audio = au;
  if (config_manager_validate(&next) != ESP_OK) return send_error(req, 400, "validation failed");
  if (s_cb.on_audio_config_update(&au) != ESP_OK) return send_error(req, 500, "apply failed");
  return send_success(req, "Audio config saved");
}

static esp_err_t handle_get_audio_state(httpd_req_t *req) {
  audio_features_t a;
  if (!s_cb.get_audio_features || s_cb.get_audio_features(&a) != ESP_OK)
    memset(&a, 0, sizeof(a));
  cJSON *r = cJSON_CreateObject();
  cJSON_AddNumberToObject(r, "raw_level",      a.raw_level);
  cJSON_AddNumberToObject(r, "rms_level",      a.rms_level);
  cJSON_AddNumberToObject(r, "peak_level",     a.peak_level);
  cJSON_AddNumberToObject(r, "smoothed_level", a.smoothed_level);
  cJSON_AddNumberToObject(r, "signal_level",   a.signal_level);
  cJSON_AddNumberToObject(r, "noise_floor",    a.noise_floor);
  cJSON_AddNumberToObject(r, "dc_offset",      a.dc_offset);
  cJSON_AddNumberToObject(r, "clipped_samples", a.clipped_samples);
  cJSON_AddNumberToObject(r, "auto_gain_x100", a.auto_gain_x100);
  cJSON_AddNumberToObject(r, "auto_gate_level", a.auto_gate_level);
  cJSON_AddNumberToObject(r, "volume_8bit",    a.volume_8bit);
  cJSON_AddNumberToObject(r, "bass_level",     a.bass_level);
  cJSON_AddNumberToObject(r, "mid_level",      a.mid_level);
  cJSON_AddNumberToObject(r, "treble_level",   a.treble_level);
  cJSON_AddBoolToObject(r,   "beat_detected",  a.beat_detected);
  cJSON_AddBoolToObject(r,   "onset_detected", a.onset_detected);
  cJSON_AddNumberToObject(r, "dominant_frequency", a.dominant_frequency);
  cJSON_AddNumberToObject(r, "spectral_centroid",  a.spectral_centroid);
  cJSON *bands = cJSON_AddArrayToObject(r, "spectrum_bands");
  for (uint8_t i = 0; i < a.band_count && i < AUDIO_SPECTRUM_BANDS_MAX; i++) {
    cJSON_AddItemToArray(bands, cJSON_CreateNumber(a.spectrum_bands[i]));
  }
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_get_audio_spectrum(httpd_req_t *req) {
  audio_features_t a;
  if (!s_cb.get_audio_features || s_cb.get_audio_features(&a) != ESP_OK)
    memset(&a, 0, sizeof(a));

  cJSON *r = cJSON_CreateObject();
  cJSON_AddNumberToObject(r, "band_count", a.band_count);
  cJSON *bands = cJSON_AddArrayToObject(r, "spectrum_bands");
  for (uint8_t i = 0; i < a.band_count && i < AUDIO_SPECTRUM_BANDS_MAX; i++) {
    cJSON_AddItemToArray(bands, cJSON_CreateNumber(a.spectrum_bands[i]));
  }
  cJSON_AddNumberToObject(r, "bass_level", a.bass_level);
  cJSON_AddNumberToObject(r, "mid_level", a.mid_level);
  cJSON_AddNumberToObject(r, "treble_level", a.treble_level);
  cJSON_AddNumberToObject(r, "volume_8bit", a.volume_8bit);
  cJSON_AddNumberToObject(r, "noise_floor", a.noise_floor);
  cJSON_AddNumberToObject(r, "auto_gain_x100", a.auto_gain_x100);
  cJSON_AddNumberToObject(r, "dominant_frequency", a.dominant_frequency);
  cJSON_AddNumberToObject(r, "spectral_centroid", a.spectral_centroid);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

/* --- MATRIX CONFIG --- */
static esp_err_t handle_get_matrix(httpd_req_t *req) {
  const app_config_t *cfg = s_cb.get_config();
  if (!cfg) return send_error(req, 500, "n/a");
  const matrix_config_t *mx = &cfg->matrix;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddBoolToObject(r,   "enabled",   mx->enabled);
  cJSON_AddNumberToObject(r, "width",     mx->width);
  cJSON_AddNumberToObject(r, "height",    mx->height);
  cJSON_AddStringToObject(r, "layout",    config_manager_matrix_layout_to_string(mx->layout));
  cJSON_AddStringToObject(r, "origin",    config_manager_matrix_origin_to_string(mx->origin));
  cJSON_AddBoolToObject(r,   "reverse_x", mx->reverse_x);
  cJSON_AddBoolToObject(r,   "reverse_y", mx->reverse_y);
  cJSON_AddBoolToObject(r,   "rotate_90", mx->rotate_90);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_post_matrix(httpd_req_t *req) {
  if (!s_cb.on_matrix_config_update) return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK) return send_error(req, 400, "bad json");
  const app_config_t *cur = s_cb.get_config();
  matrix_config_t mx = cur->matrix;
  const cJSON *v;
  v = cJSON_GetObjectItemCaseSensitive(j, "enabled");   if (cJSON_IsBool(v))   mx.enabled = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "width");     if (cJSON_IsNumber(v)) mx.width   = (uint16_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "height");    if (cJSON_IsNumber(v)) mx.height  = (uint16_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "layout");
  if (cJSON_IsString(v)) {
    matrix_layout_t l;
    if (config_manager_matrix_layout_from_string(v->valuestring, &l) == ESP_OK) mx.layout = l;
  }
  v = cJSON_GetObjectItemCaseSensitive(j, "origin");
  if (cJSON_IsString(v)) {
    matrix_origin_t o;
    if (config_manager_matrix_origin_from_string(v->valuestring, &o) == ESP_OK) mx.origin = o;
  }
  v = cJSON_GetObjectItemCaseSensitive(j, "reverse_x"); if (cJSON_IsBool(v))   mx.reverse_x = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "reverse_y"); if (cJSON_IsBool(v))   mx.reverse_y = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "rotate_90"); if (cJSON_IsBool(v))   mx.rotate_90 = cJSON_IsTrue(v);
  cJSON_Delete(j);
  app_config_t next = *cur;
  next.matrix = mx;
  if (config_manager_validate(&next) != ESP_OK) return send_error(req, 400, "validation failed");
  if (s_cb.on_matrix_config_update(&mx) != ESP_OK) return send_error(req, 500, "apply failed");
  return send_success(req, "Matrix config saved");
}

/* --- RANDOM REACTIVE / NORMAL --- */
static esp_err_t handle_post_random_reactive(httpd_req_t *req) {
  if (!s_cb.on_random_reactive_update) return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK) return send_error(req, 400, "bad json");
  const app_config_t *cur = s_cb.get_config();
  random_reactive_config_t rr = cur->random_reactive;
  const cJSON *v;
  v = cJSON_GetObjectItemCaseSensitive(j, "enabled");          if (cJSON_IsBool(v))   rr.enabled = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "interval_seconds"); if (cJSON_IsNumber(v)) rr.interval_seconds = (uint16_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "no_repeat");        if (cJSON_IsBool(v))   rr.no_repeat = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "include_strip");    if (cJSON_IsBool(v))   rr.include_strip = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "include_matrix");   if (cJSON_IsBool(v))   rr.include_matrix = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "only_favorites");   if (cJSON_IsBool(v))   rr.only_favorites = cJSON_IsTrue(v);
  cJSON_Delete(j);
  app_config_t next = *cur;
  next.random_reactive = rr;
  if (config_manager_validate(&next) != ESP_OK) return send_error(req, 400, "validation failed");
  if (s_cb.on_random_reactive_update(&rr) != ESP_OK) return send_error(req, 500, "apply failed");
  return send_success(req, "Random reactive saved");
}

static esp_err_t handle_post_random_normal(httpd_req_t *req) {
  if (!s_cb.on_random_normal_update) return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK) return send_error(req, 400, "bad json");
  const app_config_t *cur = s_cb.get_config();
  random_normal_config_t rn = cur->random_normal;
  const cJSON *v;
  v = cJSON_GetObjectItemCaseSensitive(j, "enabled");          if (cJSON_IsBool(v))   rn.enabled = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "interval_seconds"); if (cJSON_IsNumber(v)) rn.interval_seconds = (uint16_t)v->valueint;
  v = cJSON_GetObjectItemCaseSensitive(j, "no_repeat");        if (cJSON_IsBool(v))   rn.no_repeat = cJSON_IsTrue(v);
  v = cJSON_GetObjectItemCaseSensitive(j, "only_favorites");   if (cJSON_IsBool(v))   rn.only_favorites = cJSON_IsTrue(v);
  cJSON_Delete(j);
  app_config_t next = *cur;
  next.random_normal = rn;
  if (config_manager_validate(&next) != ESP_OK) return send_error(req, 400, "validation failed");
  if (s_cb.on_random_normal_update(&rn) != ESP_OK) return send_error(req, 500, "apply failed");
  return send_success(req, "Random normal saved");
}

static cJSON *random_to_json(const app_config_t *cfg) {
  cJSON *r = cJSON_CreateObject();
  const random_reactive_config_t *rr = &cfg->random_reactive;
  cJSON *rrj = cJSON_AddObjectToObject(r, "random_reactive");
  cJSON_AddBoolToObject(rrj,   "enabled",          rr->enabled);
  cJSON_AddNumberToObject(rrj, "interval_seconds", rr->interval_seconds);
  cJSON_AddBoolToObject(rrj,   "no_repeat",        rr->no_repeat);
  cJSON_AddBoolToObject(rrj,   "include_strip",    rr->include_strip);
  cJSON_AddBoolToObject(rrj,   "include_matrix",   rr->include_matrix);
  cJSON_AddBoolToObject(rrj,   "only_favorites",   rr->only_favorites);

  const random_normal_config_t *rn = &cfg->random_normal;
  cJSON *rnj = cJSON_AddObjectToObject(r, "random_normal");
  cJSON_AddBoolToObject(rnj,   "enabled",          rn->enabled);
  cJSON_AddNumberToObject(rnj, "interval_seconds", rn->interval_seconds);
  cJSON_AddBoolToObject(rnj,   "no_repeat",        rn->no_repeat);
  cJSON_AddBoolToObject(rnj,   "only_favorites",   rn->only_favorites);
  return r;
}

static void parse_random_reactive_obj(const cJSON *j,
                                      random_reactive_config_t *rr) {
  bool b;
  uint16_t u16;
  if (get_bool_obj(j, "enabled", &b)) rr->enabled = b;
  if (get_u16_obj(j, "interval_seconds", &u16)) rr->interval_seconds = u16;
  if (get_bool_obj(j, "no_repeat", &b)) rr->no_repeat = b;
  if (get_bool_obj(j, "include_strip", &b)) rr->include_strip = b;
  if (get_bool_obj(j, "include_matrix", &b)) rr->include_matrix = b;
  if (get_bool_obj(j, "only_favorites", &b)) rr->only_favorites = b;
}

static void parse_random_normal_obj(const cJSON *j, random_normal_config_t *rn) {
  bool b;
  uint16_t u16;
  if (get_bool_obj(j, "enabled", &b)) rn->enabled = b;
  if (get_u16_obj(j, "interval_seconds", &u16)) rn->interval_seconds = u16;
  if (get_bool_obj(j, "no_repeat", &b)) rn->no_repeat = b;
  if (get_bool_obj(j, "only_favorites", &b)) rn->only_favorites = b;
}

static esp_err_t handle_get_random(httpd_req_t *req) {
  const app_config_t *cfg = s_cb.get_config();
  if (!cfg) return send_error(req, 500, "n/a");
  cJSON *r = random_to_json(cfg);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_post_random(httpd_req_t *req) {
  if (!s_cb.on_random_reactive_update || !s_cb.on_random_normal_update)
    return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK) return send_error(req, 400, "bad json");

  const app_config_t *cur = s_cb.get_config();
  random_reactive_config_t rr = cur->random_reactive;
  random_normal_config_t rn = cur->random_normal;

  const cJSON *rrj = cJSON_GetObjectItemCaseSensitive(j, "random_reactive");
  const cJSON *rnj = cJSON_GetObjectItemCaseSensitive(j, "random_normal");
  if (cJSON_IsObject(rrj)) parse_random_reactive_obj(rrj, &rr);
  else parse_random_reactive_obj(j, &rr);
  if (cJSON_IsObject(rnj)) parse_random_normal_obj(rnj, &rn);

  cJSON_Delete(j);
  app_config_t next = *cur;
  next.random_reactive = rr;
  next.random_normal = rn;
  if (config_manager_validate(&next) != ESP_OK) return send_error(req, 400, "validation failed");
  if (s_cb.on_random_reactive_update(&rr) != ESP_OK)
    return send_error(req, 500, "apply reactive failed");
  if (s_cb.on_random_normal_update(&rn) != ESP_OK)
    return send_error(req, 500, "apply normal failed");
  return send_success(req, "Random config saved");
}

static esp_err_t handle_post_random_next(httpd_req_t *req) {
  operating_mode_t mode = OPERATING_MODE_REACTIVE;
  cJSON *j = NULL;
  if (req->content_len > 0 && read_json_body(req, &j) == ESP_OK) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "operating_mode");
    if (cJSON_IsString(v) && v->valuestring)
      config_manager_operating_mode_from_string(v->valuestring, &mode);
    cJSON_Delete(j);
  }
  if (s_cb.on_random_next && s_cb.on_random_next(mode) != ESP_OK)
    return send_error(req, 500, "apply failed");
  return send_success(req, "Random next requested");
}

/* --- PALETTE --- */
static esp_err_t handle_get_palettes(httpd_req_t *req) {
  cJSON *r = cJSON_CreateObject();
  cJSON *arr = cJSON_AddArrayToObject(r, "palettes");
  for (size_t i = 0; i < palette_manager_count(); i++) {
    const palette_t *p = palette_manager_get((uint8_t)i);
    if (!p) continue;
    cJSON *pj = cJSON_CreateObject();
    cJSON_AddNumberToObject(pj, "id", p->id);
    cJSON_AddStringToObject(pj, "name", p->name);
    cJSON_AddNumberToObject(pj, "color_count", p->color_count);
    cJSON *colors = cJSON_AddArrayToObject(pj, "colors");
    for (uint8_t c = 0; c < p->color_count; c++) {
      cJSON *cj = cJSON_CreateObject();
      cJSON_AddNumberToObject(cj, "r", p->colors[c].r);
      cJSON_AddNumberToObject(cj, "g", p->colors[c].g);
      cJSON_AddNumberToObject(cj, "b", p->colors[c].b);
      cJSON_AddNumberToObject(cj, "w", p->colors[c].w);
      cJSON_AddItemToArray(colors, cj);
    }
    cJSON_AddItemToArray(arr, pj);
  }
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_post_palette(httpd_req_t *req) {
  if (!s_cb.on_palette_update) return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK) return send_error(req, 400, "bad json");
  uint8_t pid = 0;
  if (!get_u8(j, "palette_id", &pid)) {
    cJSON_Delete(j);
    return send_error(req, 400, "missing palette_id");
  }
  cJSON_Delete(j);
  if (s_cb.on_palette_update(pid) != ESP_OK) return send_error(req, 500, "apply failed");
  return send_success(req, "Palette saved");
}

/* --- DISPLAY CONFIG --- */
static cJSON *display_to_json(const display_config_t *dp) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddBoolToObject(r,   "enabled",    dp->enabled);
  cJSON_AddNumberToObject(r, "brightness", dp->brightness);
  cJSON_AddStringToObject(r, "theme",      config_manager_display_theme_to_string(dp->theme));
  cJSON_AddStringToObject(r, "viewMode",   config_manager_display_view_to_string(dp->view_mode));
  cJSON_AddBoolToObject(r,   "showFps",    dp->show_fps);
  cJSON_AddBoolToObject(r,   "showWifi",   dp->show_wifi);
  return r;
}

static void parse_display_fields(const cJSON *j, display_config_t *dp) {
  bool b;
  uint8_t u8;
  const cJSON *v;
  if (get_bool_obj(j, "enabled", &b)) dp->enabled = b;
  if (get_u8(j, "brightness", &u8)) dp->brightness = u8;
  v = cJSON_GetObjectItemCaseSensitive(j, "theme");
  if (cJSON_IsString(v) && v->valuestring) {
    display_theme_t theme;
    if (config_manager_display_theme_from_string(v->valuestring, &theme) == ESP_OK) dp->theme = theme;
  }
  v = cJSON_GetObjectItemCaseSensitive(j, "viewMode");
  if (!cJSON_IsString(v)) v = cJSON_GetObjectItemCaseSensitive(j, "view_mode");
  if (!cJSON_IsString(v)) v = cJSON_GetObjectItemCaseSensitive(j, "view");
  if (cJSON_IsString(v) && v->valuestring) {
    display_view_mode_t view;
    if (config_manager_display_view_from_string(v->valuestring, &view) == ESP_OK) dp->view_mode = view;
  }
  if (get_bool_obj(j, "showFps", &b) || get_bool_obj(j, "show_fps", &b)) dp->show_fps = b;
  if (get_bool_obj(j, "showWifi", &b) || get_bool_obj(j, "show_wifi", &b)) dp->show_wifi = b;
}

static esp_err_t handle_get_display(httpd_req_t *req) {
  const app_config_t *cfg = s_cb.get_config();
  if (!cfg) return send_error(req, 500, "n/a");
  cJSON *r = display_to_json(&cfg->display);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

static esp_err_t handle_post_display(httpd_req_t *req) {
  if (!s_cb.on_display_config_update) return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK) return send_error(req, 400, "bad json");
  const app_config_t *cur = s_cb.get_config();
  display_config_t dp = cur->display;
  const cJSON *body = cJSON_GetObjectItemCaseSensitive(j, "display");
  parse_display_fields(cJSON_IsObject(body) ? body : j, &dp);
  cJSON_Delete(j);
  app_config_t next = *cur;
  next.display = dp;
  if (config_manager_validate(&next) != ESP_OK) return send_error(req, 400, "validation failed");
  if (s_cb.on_display_config_update(&dp) != ESP_OK) return send_error(req, 500, "apply failed");
  return send_success(req, "Display config saved");
}

static esp_err_t handle_post_display_view(httpd_req_t *req) {
  if (!s_cb.on_display_view_update) return send_error(req, 500, "not supported");
  cJSON *j = NULL;
  if (read_json_body(req, &j) != ESP_OK) return send_error(req, 400, "bad json");
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, "viewMode");
  if (!cJSON_IsString(v)) v = cJSON_GetObjectItemCaseSensitive(j, "view_mode");
  if (!cJSON_IsString(v)) v = cJSON_GetObjectItemCaseSensitive(j, "view");
  if (!cJSON_IsString(v) || !v->valuestring) {
    cJSON_Delete(j);
    return send_error(req, 400, "missing viewMode");
  }
  display_view_mode_t view;
  if (config_manager_display_view_from_string(v->valuestring, &view) != ESP_OK) {
    cJSON_Delete(j);
    return send_error(req, 400, "invalid viewMode");
  }
  cJSON_Delete(j);
  if (s_cb.on_display_view_update(view) != ESP_OK) return send_error(req, 500, "apply failed");
  return send_success(req, "Display view updated");
}

/* --- SYSTEM MONITOR --- */
static esp_err_t handle_get_system(httpd_req_t *req) {
  system_monitor_snapshot_t s;
  system_monitor_get_snapshot(&s);

  cJSON *r = cJSON_CreateObject();
  cJSON_AddNumberToObject(r, "uptime_ms", s.uptime_ms);
  cJSON_AddNumberToObject(r, "free_heap", s.free_heap);
  cJSON_AddNumberToObject(r, "min_free_heap", s.min_free_heap);
  cJSON_AddNumberToObject(r, "largest_free_block", s.largest_free_block);
  cJSON_AddNumberToObject(r, "render_frames", s.render_frames);
  cJSON_AddNumberToObject(r, "render_last_ms", s.render_last_ms);
  cJSON_AddNumberToObject(r, "render_max_ms", s.render_max_ms);
  cJSON_AddNumberToObject(r, "render_avg_ms_x100", s.render_avg_ms_x100);
  cJSON_AddNumberToObject(r, "render_stack_free_words", s.render_stack_free_words);
  cJSON_AddNumberToObject(r, "wifi_clients", s.wifi_clients);
  cJSON_AddBoolToObject(r, "audio_running", s.audio_running);
  cJSON_AddBoolToObject(r, "config_save_pending", s.config_save_pending);
  cJSON_AddNumberToObject(r, "config_save_count", s.config_save_count);
  cJSON_AddNumberToObject(r, "config_last_save_err", s.config_last_save_err);
  esp_err_t e = send_json(req, r, 200);
  cJSON_Delete(r);
  return e;
}

/* --- ROUTES --- */
static const httpd_uri_t k_routes[] = {

    {"/", HTTP_GET, handle_root, NULL},
    {"/api/config", HTTP_GET, handle_get_config, NULL},
    {"/api/config", HTTP_POST, handle_post_config, NULL},
    {"/api/led", HTTP_GET, handle_get_config, NULL},
    {"/api/led", HTTP_POST, handle_post_config, NULL},
    {"/api/color", HTTP_POST, handle_post_color, NULL},
    {"/api/brightness", HTTP_POST, handle_post_brightness, NULL},
    {"/api/power", HTTP_POST, handle_post_power, NULL},
    {"/api/factory-reset", HTTP_POST, handle_factory_reset, NULL},
    {"/api/animation", HTTP_GET, handle_get_animation, NULL},
    {"/api/animation", HTTP_POST, handle_post_animation, NULL},
    {"/api/animations", HTTP_GET, handle_get_animations, NULL},
    {"/api/mode", HTTP_GET, handle_get_mode, NULL},
    {"/api/mode", HTTP_POST, handle_post_mode, NULL},
    {"/api/effects", HTTP_GET, handle_get_effects, NULL},
    {"/api/effect", HTTP_POST, handle_post_effect, NULL},
    {"/api/reactive/effects", HTTP_GET, handle_get_reactive_effects, NULL},
    {"/api/reactive/effect", HTTP_POST, handle_post_reactive_effect, NULL},
    {"/api/audio/config", HTTP_GET,  handle_get_audio_config, NULL},
    {"/api/audio/config", HTTP_POST, handle_post_audio_config, NULL},
    {"/api/audio/state",  HTTP_GET,  handle_get_audio_state, NULL},
    {"/api/audio/spectrum", HTTP_GET, handle_get_audio_spectrum, NULL},
    {"/api/matrix", HTTP_GET,  handle_get_matrix, NULL},
    {"/api/matrix", HTTP_POST, handle_post_matrix, NULL},
    {"/api/random", HTTP_GET, handle_get_random, NULL},
    {"/api/random", HTTP_POST, handle_post_random, NULL},
    {"/api/random/next", HTTP_POST, handle_post_random_next, NULL},
    {"/api/random/reactive", HTTP_POST, handle_post_random_reactive, NULL},
    {"/api/random/normal",   HTTP_POST, handle_post_random_normal, NULL},
    {"/api/palettes", HTTP_GET, handle_get_palettes, NULL},
    {"/api/palette", HTTP_POST, handle_post_palette, NULL},
    {"/api/display", HTTP_GET, handle_get_display, NULL},
    {"/api/display", HTTP_POST, handle_post_display, NULL},
    {"/api/display/view", HTTP_POST, handle_post_display_view, NULL},
    {"/api/system", HTTP_GET, handle_get_system, NULL},
};

esp_err_t web_server_start(const web_server_callbacks_t *cb) {

  if (!cb || !cb->on_config_update || !cb->get_config)
    return ESP_ERR_INVALID_ARG;
  if (s_server)
    return ESP_ERR_INVALID_STATE;
  s_cb = *cb;
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.max_uri_handlers = sizeof(k_routes) / sizeof(k_routes[0]);
  cfg.lru_purge_enable = true;
  esp_err_t err = httpd_start(&s_server, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "start fail");
    return err;
  }
  for (size_t i = 0; i < sizeof(k_routes) / sizeof(k_routes[0]); i++) {
    err = httpd_register_uri_handler(s_server, &k_routes[i]);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "reg %s fail", k_routes[i].uri);
      httpd_stop(s_server);
      s_server = NULL;
      return err;
    }
  }
  ESP_LOGI(TAG, "started on port %d", cfg.server_port);
  return ESP_OK;
}
esp_err_t web_server_stop(void) {
  if (!s_server)
    return ESP_OK;
  esp_err_t e = httpd_stop(s_server);
  s_server = NULL;
  return e;
}
