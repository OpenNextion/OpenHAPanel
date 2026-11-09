#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_SYSTEM_H

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#define HA_UI_TTF_PATH "/Littlefs/fonts/NotoSansKR-Regular.ttf"

/* FT_New_Memory_Face does not copy the data. Keep it until FreeType stops. */
static FT_Byte *s_font_data;
static FT_Long s_font_size;

/* FreeType keeps this allocator pointer for the lifetime of its library. */
static void *ft_psram_alloc(FT_Memory memory, long size)
{
    (void)memory;
    return size > 0 ? heap_caps_malloc((size_t)size,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
}

static void *ft_psram_realloc(FT_Memory memory, long old_size, long new_size, void *block)
{
    (void)memory;
    (void)old_size;
    if (new_size <= 0) {
        heap_caps_free(block);
        return NULL;
    }
    return heap_caps_realloc(block, (size_t)new_size,
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void ft_psram_free(FT_Memory memory, void *block)
{
    (void)memory;
    heap_caps_free(block);
}

static struct FT_MemoryRec_ s_ft_psram_memory = {
    .user = NULL,
    .alloc = ft_psram_alloc,
    .free = ft_psram_free,
    .realloc = ft_psram_realloc,
};

FT_Error ha_freetype_init_psram(FT_Library *library)
{
    FT_Error error = FT_New_Library(&s_ft_psram_memory, library);
    if (error) return error;

    FT_Add_Default_Modules(*library);
    FT_Set_Default_Properties(*library);
    ESP_LOGI("freetype_psram", "FreeType allocations routed to PSRAM");
    return 0;
}

FT_Error ha_freetype_done_psram(FT_Library library)
{
    FT_Error error = FT_Done_Library(library);
    if (!error) {
        heap_caps_free(s_font_data);
        s_font_data = NULL;
        s_font_size = 0;
    }
    return error;
}

FT_Error ha_freetype_new_face_psram(FT_Library library, const char *pathname,
                                    FT_Long face_index, FT_Face *face)
{
    if (!pathname || !face) return FT_Err_Invalid_Argument;

    if (strcmp(pathname, HA_UI_TTF_PATH) != 0) {
        FT_Open_Args args = {
            .flags = FT_OPEN_PATHNAME,
            .pathname = (char *)pathname,
        };
        return FT_Open_Face(library, &args, face_index, face);
    }

    if (!s_font_data) {
        FILE *file = fopen(pathname, "rb");
        if (!file) return FT_Err_Cannot_Open_Resource;

        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return FT_Err_Cannot_Open_Resource;
        }
        long size = ftell(file);
        if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
            fclose(file);
            return FT_Err_Cannot_Open_Resource;
        }

        FT_Byte *data = heap_caps_malloc((size_t)size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!data) {
            fclose(file);
            ESP_LOGE("freetype_psram", "No contiguous PSRAM for %ld-byte font", size);
            return FT_Err_Out_Of_Memory;
        }
        size_t read_size = fread(data, 1, (size_t)size, file);
        fclose(file);
        if (read_size != (size_t)size) {
            heap_caps_free(data);
            return FT_Err_Cannot_Open_Resource;
        }
        s_font_data = data;
        s_font_size = size;
        ESP_LOGI("freetype_psram", "TTF loaded into PSRAM: %ld bytes", size);
    }

    return FT_New_Memory_Face(library, s_font_data, s_font_size, face_index, face);
}
