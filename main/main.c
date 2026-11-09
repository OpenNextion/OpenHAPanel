#include "app.h"
#include "bsp_lcd.h"
#include "check_ram.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_storage.h"
#include "nvs_flash.h"
#include "littlefs.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "miniz.h"
#include "ha_ui_font_data.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static void dump_start_reason(void);
static void dump_ota_partition_info(void);
static const char *ota_image_state_to_string(esp_ota_img_states_t state);
static void setup_cjson_hooks(void);
static void *psram_malloc(size_t size);
static void psram_free(void *ptr);
static void install_ui_font(void);

static const char *TAG = "main";

#define FW_STATE_NVS_NS "esp32_cfg"
#define FW_STATE_NVS_KEY_ESP32_VERSION "esp32_ver"
#define FW_STATE_NVS_KEY_ESP32_TYPE "esp32_type"

static esp_err_t save_esp32_info_to_nvs(void)
{
    const nvs_storage_write_item_t items[] = {
        {
            .key = FW_STATE_NVS_KEY_ESP32_VERSION,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = CONFIG_FIRMWARE_VERSION_STRING,
        },
        {
            .key = FW_STATE_NVS_KEY_ESP32_TYPE,
            .type = NVS_STORAGE_TYPE_STRING,
            .value.string = CONFIG_FIRMWARE_TYPE_STRING,
        },
    };
    return nvs_storage_write(FW_STATE_NVS_NS, items, sizeof(items) / sizeof(items[0]));
}
/**
 * @brief Initialize essential ESP32 system components including NVS, BT, event loop, and network stack.
 *
 * This function initializes the Non-Volatile Storage (NVS), releases unused Bluetooth memory,
 * creates the default event loop, and initializes the TCP/IP and Wi-Fi interfaces.
 */
static void esp_init(void)
{
    /* initialize the nvs component */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_stats_t nvs_stats = {0};
    nvs_get_stats(NULL, &nvs_stats);
    ESP_LOGI(TAG, "NVS Stats: Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)", nvs_stats.used_entries,
             nvs_stats.free_entries, nvs_stats.total_entries);

    ret = save_esp32_info_to_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save ESP32 firmware info: %s", esp_err_to_name(ret));
    }
    // ESP_ERROR_CHECK(esp_bt_mem_release(ESP_BT_MODE_CLASSIC_BT));
    /* initialize the default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Initialize the TCP/IP protocol stack and WI-FI interface */
    ESP_ERROR_CHECK(esp_netif_init());
    /* Initialize the LittleFS file system */
    ESP_ERROR_CHECK(little_file_system_init());
    install_ui_font();
}

#define UI_FONT_DIR ROOT_PATH "/fonts"
#define UI_FONT_PATH UI_FONT_DIR "/NotoSansKR-Regular.ttf"
#define UI_FONT_TEMP UI_FONT_PATH ".tmp"

extern const unsigned char _binary_ha_ui_font_zlib_start[];
extern const unsigned char _binary_ha_ui_font_zlib_end[];

typedef struct {
    FILE *file;
    size_t size;
    uint32_t hash;
    int write_errno;
} ui_font_writer_t;

static uint32_t ui_font_hash(uint32_t hash, const unsigned char *data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ data[i]) * 0x01000193u;
    }
    return hash;
}

static bool ui_font_is_current(void)
{
    struct stat st;
    if (stat(UI_FONT_PATH, &st) != 0 ||
        st.st_size != HA_UI_FONT_SIZE) {
        return false;
    }

    FILE *file = fopen(UI_FONT_PATH, "rb");
    if (!file) return false;

    unsigned char buffer[4096];
    uint32_t hash = 0x811c9dc5u;
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        hash = ui_font_hash(hash, buffer, count);
    }
    bool valid = !ferror(file) && hash == HA_UI_FONT_FNV1A;
    fclose(file);
    return valid;
}

static int ui_font_write(const void *data, int len, void *user)
{
    ui_font_writer_t *writer = user;
    if (len < 0 || (size_t)len > HA_UI_FONT_SIZE - writer->size) {
        writer->write_errno = EFBIG;
        return 0;
    }
    if (fwrite(data, 1, (size_t)len, writer->file) != (size_t)len) {
        writer->write_errno = errno;
        return 0;
    }
    writer->size += (size_t)len;
    writer->hash = ui_font_hash(writer->hash, data, (size_t)len);
    return 1;
}

static tinfl_status ui_font_inflate(const unsigned char *packed, size_t packed_size,
                                    ui_font_writer_t *writer, size_t *consumed)
{
    tinfl_decompressor *inflater = heap_caps_calloc(1, sizeof(*inflater),
                                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    unsigned char *dictionary = heap_caps_malloc(TINFL_LZ_DICT_SIZE,
                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!inflater || !dictionary) {
        ESP_LOGE(TAG, "Unable to allocate PSRAM for font inflater (state=%u dictionary=%u)",
                 (unsigned)sizeof(*inflater), (unsigned)TINFL_LZ_DICT_SIZE);
        heap_caps_free(inflater);
        heap_caps_free(dictionary);
        return TINFL_STATUS_FAILED;
    }

    tinfl_init(inflater);
    size_t input_pos = 0;
    size_t output_pos = 0;
    tinfl_status status;
    do {
        size_t input_size = packed_size - input_pos;
        size_t output_size = TINFL_LZ_DICT_SIZE - output_pos;
        status = tinfl_decompress(inflater, packed + input_pos, &input_size,
                                  dictionary, dictionary + output_pos, &output_size,
                                  TINFL_FLAG_PARSE_ZLIB_HEADER);
        input_pos += input_size;
        if (output_size && !ui_font_write(dictionary + output_pos, (int)output_size, writer)) {
            status = TINFL_STATUS_FAILED;
            break;
        }
        output_pos += output_size;
        if (output_pos == TINFL_LZ_DICT_SIZE) output_pos = 0;
        if (status == TINFL_STATUS_NEEDS_MORE_INPUT && input_pos == packed_size) break;
        if (status == TINFL_STATUS_HAS_MORE_OUTPUT && output_size == 0) break;
    } while (status == TINFL_STATUS_HAS_MORE_OUTPUT || status == TINFL_STATUS_NEEDS_MORE_INPUT);

    *consumed = input_pos;
    heap_caps_free(dictionary);
    heap_caps_free(inflater);
    return status;
}

static void install_ui_font(void)
{
    if (ui_font_is_current()) return;
    if (mkdir(UI_FONT_DIR, 0777) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "Unable to create font directory: errno=%d", errno);
        return;
    }

    /* Free the old copy first: the 3968 KiB files partition cannot hold two
     * copies of this TTF alongside its user settings. */
    unlink(UI_FONT_TEMP);
    unlink(UI_FONT_PATH);
    FILE *file = fopen(UI_FONT_TEMP, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Unable to create UI font: errno=%d", errno);
        return;
    }

    ui_font_writer_t writer = {.file = file, .size = 0, .hash = 0x811c9dc5u};
    size_t packed_size = (size_t)(_binary_ha_ui_font_zlib_end -
                                   _binary_ha_ui_font_zlib_start);
    size_t consumed = 0;
    tinfl_status result = ui_font_inflate(_binary_ha_ui_font_zlib_start,
                                          packed_size, &writer, &consumed);
    bool ok = result == TINFL_STATUS_DONE && consumed == packed_size &&
              writer.size == HA_UI_FONT_SIZE &&
              writer.hash == HA_UI_FONT_FNV1A;
    int flush_result = fflush(file);
    int flush_errno = flush_result == 0 ? 0 : errno;
    int close_result = fclose(file);
    int close_errno = close_result == 0 ? 0 : errno;
    if (flush_result != 0 || close_result != 0) ok = false;
    if (!ok) {
        unlink(UI_FONT_TEMP);
        ESP_LOGE(TAG, "Unable to install UI font: inflate=%d input=%u/%u output=%u/%u hash=%08lx/%08lx write_errno=%d flush_errno=%d close_errno=%d",
                 result, (unsigned)consumed, (unsigned)packed_size,
                 (unsigned)writer.size, (unsigned)HA_UI_FONT_SIZE,
                 (unsigned long)writer.hash, (unsigned long)HA_UI_FONT_FNV1A,
                 writer.write_errno, flush_errno, close_errno);
        return;
    }
    if (rename(UI_FONT_TEMP, UI_FONT_PATH) != 0) {
        int rename_errno = errno;
        unlink(UI_FONT_TEMP);
        ESP_LOGE(TAG, "Unable to rename UI font: errno=%d", rename_errno);
        return;
    }
    ESP_LOGI(TAG, "Installed UI TTF (%u bytes)", (unsigned)writer.size);
}

void app_main(void)
{
    esp_err_t boot_screen_err = bsp_lcd_boot_screen_show();
    if (boot_screen_err != ESP_OK) {
        ESP_LOGW(TAG, "Unable to display startup page: %s", esp_err_to_name(boot_screen_err));
    }
    dump_start_reason();
    ESP_LOGI(TAG, "Device model: %s", CONFIG_DEVICE_MODEL_STRING);
    setup_cjson_hooks();
    esp_init();
    dump_ota_partition_info();
    check_ram_timer_init();
    app_init();
}

/**
 * @brief Convert an ESP-IDF OTA image state into a log-friendly string.
 */
static const char *ota_image_state_to_string(esp_ota_img_states_t state)
{
    switch (state) {
    case ESP_OTA_IMG_NEW:
        return "new";
    case ESP_OTA_IMG_PENDING_VERIFY:
        return "pending_verify";
    case ESP_OTA_IMG_VALID:
        return "valid";
    case ESP_OTA_IMG_INVALID:
        return "invalid";
    case ESP_OTA_IMG_ABORTED:
        return "aborted";
    case ESP_OTA_IMG_UNDEFINED:
    default:
        return "undefined";
    }
}

/**
 * @brief Log the running, configured boot, next update, and last invalid OTA partitions.
 *
 * This is startup diagnostics only. It deliberately does not change OTA image
 * state; the firmware_update component performs first-boot verification.
 */
static void dump_ota_partition_info(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *last_invalid = esp_ota_get_last_invalid_partition();

    if (running) {
        ESP_LOGI(TAG, "Running OTA partition: label=%s address=0x%08lx size=%lu", running->label,
                 (unsigned long)running->address, (unsigned long)running->size);

        esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
        esp_err_t err = esp_ota_get_state_partition(running, &state);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Running OTA image state: %s", ota_image_state_to_string(state));
        } else {
            ESP_LOGW(TAG, "Unable to read running OTA image state: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "Running OTA partition is unavailable");
    }

    if (boot) {
        ESP_LOGI(TAG, "Configured boot partition: label=%s address=0x%08lx", boot->label,
                 (unsigned long)boot->address);
    } else {
        ESP_LOGW(TAG, "Configured boot partition is unavailable");
    }

    if (next) {
        ESP_LOGI(TAG, "Next OTA partition: label=%s address=0x%08lx size=%lu", next->label,
                 (unsigned long)next->address, (unsigned long)next->size);
    } else {
        ESP_LOGE(TAG, "No writable OTA partition is available");
    }

    if (last_invalid) {
        ESP_LOGW(TAG, "Last invalid OTA partition: label=%s address=0x%08lx", last_invalid->label,
                 (unsigned long)last_invalid->address);
    }
}

/**
 * @brief Dump start reason.
 */
static void dump_start_reason(void)
{
    esp_reset_reason_t reason = esp_reset_reason();

    switch (reason) {
    case ESP_RST_POWERON:
        ESP_LOGE(TAG, "Reset reason: Power-on restart");
        break;
    case ESP_RST_EXT:
        ESP_LOGE(TAG, "Reset Reason: external pin (not applicable for ESP32)");
        break;
    case ESP_RST_SW:
        ESP_LOGE(TAG, "Reset Reason: Software Reboot (via esp_restart)");
        break;
    case ESP_RST_PANIC:
        ESP_LOGE(TAG, "Reset Reason: Software Abnormality/Panic Restart");
        break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        ESP_LOGE(TAG, "Reset Reason: Watch Dog Reset");
        break;
    case ESP_RST_DEEPSLEEP:
        ESP_LOGE(TAG, "Reset reason: Deep-sleep wake restart");
        break;
    case ESP_RST_BROWNOUT:
        ESP_LOGE(TAG, "Reset reason: undervoltage (browning) restart");
        break;
    case ESP_RST_SDIO:
        ESP_LOGE(TAG, "Reset reason: Reset over SDIO");
        break;
    case ESP_RST_USB:
        ESP_LOGE(TAG, "Reset reason: by USB peripheral");
        break;
    case ESP_RST_JTAG:
        ESP_LOGE(TAG, "Reset reason: by JTAG");
        break;
    default:
        ESP_LOGE(TAG, "Reset Reason: Unknown (%d)", reason);
    }
}

/**
 * @brief Set custom memory allocation hooks for cJSON using PSRAM-capable functions.
 */
static void setup_cjson_hooks(void)
{
    cJSON_Hooks hooks = {
        .malloc_fn = psram_malloc,
        .free_fn = psram_free,
    };
    cJSON_InitHooks(&hooks);
}

/**
 * @brief Allocate memory with preference for PSRAM; fallback to internal RAM if needed.
 *
 * @param size Size in bytes to allocate.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static void *psram_malloc(size_t size)
{
    /* PSRAM is tried first, and internal memory is rolled back if it fails */
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ESP_LOGI(TAG,
                 "PSRAM allocation failed! Requested size: %d bytes, Available memory: %d bytes, Largest free block: "
                 "%d bytes",
                 size, heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

        /* There may be an abnormality due to the unstable power supply, so try to distribute it again */
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr == NULL) {
            /* Failed again to allocate space from internal memory */
            ESP_LOGW(TAG, "PSRAM error malloc MALLOC_CAP_INTERNAL");
            ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
        }
    }

    return ptr;
}

/**
 * @brief Free memory previously allocated using ps_ram_malloc or ps_ram_realloc.
 *
 * @param ptr Pointer to the memory block to free.
 */
static void psram_free(void *ptr)
{
    heap_caps_free(ptr);
}
