#include "ha_ui_font.h"

#include "assets/ha_ui_assets.h"
#include "esp_log.h"
#include "font_render_worker.h"

#define HA_UI_TTF_PATH "/Littlefs/fonts/NotoSansKR-Regular.ttf"

typedef struct {
    uint32_t size;
    lv_font_t *regular;
    lv_font_t *medium;
} ha_ui_ttf_entry_t;

static ha_ui_ttf_entry_t s_ttf_fonts[] = {
    {.size = 12}, {.size = 14}, {.size = 16}, {.size = 20},
    {.size = 24}, {.size = 30}, {.size = 36},
};

typedef struct {
    uint32_t size;
    bool medium;
    const lv_font_t *source;
    lv_font_t font;
    bool initialized;
} ha_ui_bitmap_entry_t;

/* Match the generated font's size and weight exactly. Missing combinations
 * continue to use FreeType directly. */
static ha_ui_bitmap_entry_t s_bitmap_fonts[] = {
    {.size = 12, .medium = false, .source = &ha_font_misan_regular_12},
    {.size = 14, .medium = false, .source = &ha_font_misan_regular_14},
    {.size = 16, .medium = false, .source = &ha_font_misan_regular_16},
    {.size = 20, .medium = false, .source = &ha_font_misan_regular_20},
    {.size = 12, .medium = true, .source = &ha_font_misan_medium_12},
    {.size = 16, .medium = true, .source = &ha_font_misan_medium_16},
    {.size = 20, .medium = true, .source = &ha_font_misan_medium_20},
    {.size = 24, .medium = true, .source = &ha_font_misan_medium_24},
    {.size = 30, .medium = true, .source = &ha_font_misan_medium_30},
    {.size = 36, .medium = true, .source = &ha_font_misan_medium_36},
};

static lv_font_t *ha_ui_ttf_get(uint32_t size, bool medium)
{
    for (size_t i = 0; i < sizeof(s_ttf_fonts) / sizeof(s_ttf_fonts[0]); i++) {
        ha_ui_ttf_entry_t *entry = &s_ttf_fonts[i];
        if (entry->size != size) continue;

        lv_font_t **slot = medium ? &entry->medium : &entry->regular;
        if (!*slot) {
            /* Only a Regular TTF is bundled; bold emulates Medium. */
            *slot = lv_freetype_font_create(HA_UI_TTF_PATH,
                                            LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                            size,
                                            medium ? LV_FREETYPE_FONT_STYLE_BOLD
                                                   : LV_FREETYPE_FONT_STYLE_NORMAL);
            if (*slot && !ha_font_render_worker_attach(*slot)) {
                lv_freetype_font_delete(*slot);
                *slot = NULL;
            }
            if (!*slot) {
                ESP_LOGE("ha_ui_font", "Unable to create %s %u px TTF font",
                         medium ? "medium" : "regular", (unsigned)size);
            }
        }
        return *slot;
    }

    ESP_LOGE("ha_ui_font", "Unsupported TTF font size: %u", (unsigned)size);
    return NULL;
}

static const lv_font_t *ha_ui_bitmap_get(ha_ui_bitmap_entry_t *entry)
{
    if (!entry->initialized) {
        /* Generated fonts are const, so copy the descriptor before setting
         * LVGL's per-glyph fallback. Glyph data remains shared in flash. */
        entry->font = *entry->source;
        entry->font.fallback = ha_ui_ttf_get(entry->size, entry->medium);
        entry->initialized = true;
    }
    return &entry->font;
}

static const lv_font_t *ha_ui_font_get(uint32_t size, bool medium)
{
    for (size_t i = 0; i < sizeof(s_bitmap_fonts) / sizeof(s_bitmap_fonts[0]); i++) {
        ha_ui_bitmap_entry_t *entry = &s_bitmap_fonts[i];
        if (entry->size == size && entry->medium == medium) {
            return ha_ui_bitmap_get(entry);
        }
    }

    lv_font_t *font = ha_ui_ttf_get(size, medium);
    return font ? font : LV_FONT_DEFAULT;
}

const lv_font_t *ha_ui_font_regular(uint32_t size)
{
    return ha_ui_font_get(size, false);
}

const lv_font_t *ha_ui_font_medium(uint32_t size)
{
    return ha_ui_font_get(size, true);
}

