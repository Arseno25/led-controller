#include "effect_registry.h"
#include <string.h>
#include "esp_random.h"

static const effect_registry_item_t s_effects[] = {
    {0,  "solid",                  "Solid",                  EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {1,  "blink",                  "Blink",                  EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {2,  "breathing",              "Breathing",              EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {3,  "rainbow",                "Rainbow",                EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {4,  "color_wipe",             "Color Wipe",             EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {5,  "running_dot",            "Running Dot",            EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {6,  "comet",                  "Comet",                  EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {7,  "meteor_rain",            "Meteor Rain",            EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {8,  "theater_chase",          "Theater Chase",          EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {9,  "scanner",                "Scanner",                EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {10, "twinkle",                "Twinkle",                EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {11, "sparkle",                "Sparkle",                EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {12, "fire",                   "Fire",                   EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {13, "police_light",           "Police Light",           EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {14, "neon_flicker",           "Neon Flicker",           EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {15, "gradient_flow",          "Gradient Flow",          EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {16, "wave",                   "Wave",                   EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {17, "confetti",               "Confetti",               EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {18, "pulse",                  "Pulse",                  EFFECT_CATEGORY_NORMAL,          false, false, true,  true, NULL},
    {19, "custom",                 "Custom",                 EFFECT_CATEGORY_NORMAL,          false, false, true,  false, NULL},

    {100, "reactive_vu_bar",        "Reactive VU Bar",        EFFECT_CATEGORY_REACTIVE,        false, true,  true,  true, NULL},
    {101, "reactive_pulse",         "Reactive Pulse",         EFFECT_CATEGORY_REACTIVE,        false, true,  true,  true, NULL},
    {102, "reactive_beat_flash",    "Reactive Beat Flash",    EFFECT_CATEGORY_REACTIVE,        false, true,  true,  true, NULL},
    {103, "reactive_spark",         "Reactive Spark",         EFFECT_CATEGORY_REACTIVE,        false, true,  true,  true, NULL},
    {104, "reactive_comet",         "Reactive Comet",         EFFECT_CATEGORY_REACTIVE,        false, true,  true,  true, NULL},
    {105, "reactive_chase",         "Reactive Chase",         EFFECT_CATEGORY_REACTIVE,        false, true,  true,  true, NULL},
    {106, "reactive_spectrum_bars", "Reactive Spectrum Bars", EFFECT_CATEGORY_REACTIVE,        false, true,  true,  true, NULL},
    {107, "reactive_bass_hit",      "Reactive Bass Hit",      EFFECT_CATEGORY_REACTIVE,        false, true,  true,  true, NULL},

    {200, "matrix_rain",            "Matrix Rain",            EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {201, "matrix_rainbow",         "Matrix Rainbow",         EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {202, "matrix_fire",            "Matrix Fire",            EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {203, "matrix_plasma",          "Matrix Plasma",          EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {204, "matrix_lava",            "Matrix Lava",            EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {205, "matrix_aurora",          "Matrix Aurora",          EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {206, "matrix_ripple",          "Matrix Ripple",          EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {207, "matrix_starfield",       "Matrix Starfield",       EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {208, "matrix_waterfall",       "Matrix Waterfall",       EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {209, "matrix_blocks",          "Matrix Blocks",          EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {210, "matrix_noise_flow",      "Matrix Noise Flow",      EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},
    {211, "matrix_spiral",          "Matrix Spiral",          EFFECT_CATEGORY_MATRIX,          true,  false, true,  true, NULL},

    {300, "matrix_spectrum_bars",   "Matrix Spectrum Bars",   EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {301, "matrix_center_vu",       "Matrix Center VU",       EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {302, "matrix_bass_pulse",      "Matrix Bass Pulse",      EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {303, "matrix_audio_ripple",    "Matrix Audio Ripple",    EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {304, "matrix_beat_flash",      "Matrix Beat Flash",      EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {305, "matrix_spark_field",     "Matrix Spark Field",     EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {306, "matrix_audio_plasma",    "Matrix Audio Plasma",    EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {307, "matrix_fire_eq",         "Matrix Fire EQ",         EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {308, "matrix_bass_tunnel",     "Matrix Bass Tunnel",     EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {309, "matrix_treble_rain",     "Matrix Treble Rain",     EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {310, "matrix_mid_wave",        "Matrix Mid Wave",        EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {311, "matrix_audio_blocks",    "Matrix Audio Blocks",    EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {312, "matrix_reactive_aurora", "Matrix Reactive Aurora", EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {313, "matrix_reactive_spiral", "Matrix Reactive Spiral", EFFECT_CATEGORY_REACTIVE_MATRIX, true,  true,  true,  true, NULL},
    {314, "matrix_spectrum_rainbow", "Matrix Spectrum Rainbow", EFFECT_CATEGORY_REACTIVE_MATRIX, true, true, true, true, NULL},
    {315, "matrix_spectrum_mirror",  "Matrix Spectrum Mirror",  EFFECT_CATEGORY_REACTIVE_MATRIX, true, true, true, true, NULL},
    {316, "matrix_spectrum_peaks",   "Matrix Spectrum Peaks",   EFFECT_CATEGORY_REACTIVE_MATRIX, true, true, true, true, NULL},
    {317, "matrix_spectrum_waterfall", "Matrix Spectrum Waterfall", EFFECT_CATEGORY_REACTIVE_MATRIX, true, true, true, true, NULL},
    {318, "matrix_segment_vu",       "Matrix Segment VU",       EFFECT_CATEGORY_REACTIVE_MATRIX, true, true, true, true, NULL},
};

static const char *s_category_names[EFFECT_CATEGORY_MAX] = {
    [EFFECT_CATEGORY_NORMAL] = "normal",
    [EFFECT_CATEGORY_REACTIVE] = "reactive",
    [EFFECT_CATEGORY_MATRIX] = "matrix",
    [EFFECT_CATEGORY_REACTIVE_MATRIX] = "reactive_matrix",
};

const effect_registry_item_t *effect_registry_get_all(size_t *count)
{
    if (count) *count = sizeof(s_effects) / sizeof(s_effects[0]);
    return s_effects;
}

const effect_registry_item_t *effect_registry_get_by_name(const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof(s_effects) / sizeof(s_effects[0]); i++) {
        if (strcmp(s_effects[i].name, name) == 0) return &s_effects[i];
    }
    return NULL;
}

const effect_registry_item_t *effect_registry_get_random(effect_category_t category,
                                                         const char *current_name,
                                                         bool no_repeat)
{
    if ((int)category < 0 || category >= EFFECT_CATEGORY_MAX) return NULL;

    size_t matches = 0;
    for (size_t i = 0; i < sizeof(s_effects) / sizeof(s_effects[0]); i++) {
        if (s_effects[i].category == category && s_effects[i].supports_random) matches++;
    }
    if (matches == 0) return NULL;

    const effect_registry_item_t *pick = NULL;
    for (int tries = 0; tries < 12; tries++) {
        size_t nth = esp_random() % matches;
        for (size_t i = 0; i < sizeof(s_effects) / sizeof(s_effects[0]); i++) {
            if (s_effects[i].category != category || !s_effects[i].supports_random) continue;
            if (nth-- == 0) {
                pick = &s_effects[i];
                break;
            }
        }
        if (!no_repeat || !current_name || !pick || strcmp(pick->name, current_name) != 0) {
            break;
        }
    }
    return pick;
}

const char *effect_registry_category_to_string(effect_category_t category)
{
    if ((int)category < 0 || category >= EFFECT_CATEGORY_MAX) return "normal";
    return s_category_names[category];
}

bool effect_registry_category_from_string(const char *name, effect_category_t *out)
{
    if (!name || !out) return false;
    for (int i = 0; i < EFFECT_CATEGORY_MAX; i++) {
        if (strcmp(name, s_category_names[i]) == 0) {
            *out = (effect_category_t)i;
            return true;
        }
    }
    return false;
}
