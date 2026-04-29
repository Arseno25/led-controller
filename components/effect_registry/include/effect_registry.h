#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EFFECT_CATEGORY_NORMAL = 0,
    EFFECT_CATEGORY_REACTIVE,
    EFFECT_CATEGORY_MATRIX,
    EFFECT_CATEGORY_REACTIVE_MATRIX,
    EFFECT_CATEGORY_MAX
} effect_category_t;

typedef struct {
    uint16_t id;
    const char *name;
    const char *label;
    effect_category_t category;
    bool requires_matrix;
    bool requires_audio;
    bool supports_palette;
    bool supports_random;
    void (*render)(void *ctx, uint32_t delta_ms);
} effect_registry_item_t;

const effect_registry_item_t *effect_registry_get_all(size_t *count);
const effect_registry_item_t *effect_registry_get_by_name(const char *name);
const effect_registry_item_t *effect_registry_get_random(effect_category_t category,
                                                         const char *current_name,
                                                         bool no_repeat);

const char *effect_registry_category_to_string(effect_category_t category);
bool effect_registry_category_from_string(const char *name, effect_category_t *out);

#ifdef __cplusplus
}
#endif
